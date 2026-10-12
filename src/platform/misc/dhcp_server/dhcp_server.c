#include "platform/dhcp_server.h"

#include "dhcp_leases.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "lwip/sockets.h"

// #define DEBUG_NAME "dhcpd"
#include "debug.h"

#define SERVER_PORT   67
#define CLIENT_PORT   68
#define POOL_SIZE     DHCP_POOL_SIZE
#define LEASE_SECONDS 3600

// The fixed BOOTP header and the magic cookie that opens the options.
#define BOOTP_FIXED_LEN 236
#define OPTIONS_OFFSET  240
#define REPLY_LEN       300

// How long the task waits for a packet before looking at whether it was stopped.
#define POLL_MS 200

enum { MSG_DISCOVER = 1, MSG_OFFER = 2, MSG_REQUEST = 3, MSG_ACK = 5, MSG_NAK = 6, MSG_RELEASE = 7 };

// Everything one server needs. It lives from dhcp_server_start() to
// dhcp_server_stop(); the task gets it as its argument.
struct dhcp_server_state {
    struct netif      *netif;
    int                fd;
    struct dhcp_leases leases;

    // NULL while the server runs. dhcp_server_stop() creates it over stop_storage
    // and waits on it; the task sees it, gives it back and deletes itself.
    volatile SemaphoreHandle_t stop_requested;
    StaticSemaphore_t          stop_storage;

    uint8_t request_buf[REPLY_LEN * 2];
    uint8_t response_buf[REPLY_LEN];
};

// One server at a time.
static struct dhcp_server_state *s_state;

// Host byte order.
static uint32_t server_addr(const struct netif *netif) { return lwip_ntohl(ip4_addr_get_u32(netif_ip4_addr(netif))); }
static uint32_t netmask(const struct netif *netif) { return lwip_ntohl(ip4_addr_get_u32(netif_ip4_netmask(netif))); }
static uint32_t slot_addr(const struct dhcp_server_state *state, int slot) {
    return server_addr(state->netif) + 1 + (uint32_t)slot;
}

static void put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

// ---- Replies ---------------------------------------------------------------

static void put_option(uint8_t **o, uint8_t code, uint8_t len, const void *data) {
    (*o)[0] = code;
    (*o)[1] = len;
    memcpy(*o + 2, data, len);
    *o += 2 + len;
}

static void put_option32(uint8_t **o, uint8_t code, uint32_t v) {
    uint8_t b[4];
    put32(b, v);
    put_option(o, code, 4, b);
}

// Always broadcast: the client has no address to answer yet.
static void reply(struct dhcp_server_state *state, const uint8_t *req, uint8_t type, uint32_t yiaddr) {
    const uint32_t server = server_addr(state->netif);
    uint8_t       *m      = state->response_buf;
    const size_t   m_sz   = sizeof(state->response_buf);
    memset(m, 0, m_sz);

    m[0] = 2; // BOOTREPLY
    m[1] = 1; // Ethernet
    m[2] = 6;
    memcpy(m + 4, req + 4, 4);   // transaction id
    memcpy(m + 10, req + 10, 2); // flags
    put32(m + 16, yiaddr);
    put32(m + 20, server);
    memcpy(m + 28, req + 28, 16); // client hardware address
    m[BOOTP_FIXED_LEN]     = 0x63;
    m[BOOTP_FIXED_LEN + 1] = 0x82;
    m[BOOTP_FIXED_LEN + 2] = 0x53;
    m[BOOTP_FIXED_LEN + 3] = 0x63;

    uint8_t *o = m + OPTIONS_OFFSET;
    put_option(&o, 53, 1, &type);
    put_option32(&o, 54, server);
    if (type != MSG_NAK) {
        put_option32(&o, 51, LEASE_SECONDS);
        put_option32(&o, 1, netmask(state->netif));
        put_option32(&o, 3, server);
        put_option32(&o, 6, server);
    }
    *o++ = 255;

    struct sockaddr_in to = {
        .sin_len    = sizeof(to),
        .sin_family = AF_INET,
        .sin_port   = lwip_htons(CLIENT_PORT),
        .sin_addr   = {.s_addr = PP_HTONL(IPADDR_BROADCAST)},
        .sin_zero   = {0},
    };
    lwip_sendto(state->fd, m, (size_t)(o - m), 0, (struct sockaddr *)&to, sizeof(to));
}

// ---- Requests --------------------------------------------------------------

static void on_message(struct dhcp_server_state *state, const uint8_t *req, uint16_t len) {
    uint8_t  type      = 0;
    uint32_t requested = 0;

    // A malformed option list ends the walk, and the message is then ignored.
    for (uint16_t i = OPTIONS_OFFSET; i < len;) {
        uint8_t code = req[i];
        if (code == 255) break;
        if (code == 0) {
            i++;
            continue;
        }
        if (i + 2 > len) return;

        uint8_t n = req[i + 1];
        if (i + 2 + n > len) return;

        if (code == 53 && n == 1) type = req[i + 2];
        if (code == 50 && n == 4) requested = get32(req + i + 2);
        i += 2 + n;
    }

    const uint8_t *mac  = req + 28;
    int            slot = dhcp_leases_find(&state->leases, mac);

    switch (type) {
        case MSG_DISCOVER:
            // A full pool gives the address of whoever asked longest ago.
            if (slot < 0) slot = dhcp_leases_take(&state->leases, mac);
            reply(state, req, MSG_OFFER, slot_addr(state, slot));
            break;

        case MSG_REQUEST: {
            // The address asked for is in option 50 while selecting, in ciaddr
            // while renewing.
            if (!requested) requested = get32(req + 12);

            // A client back from a restart asks for the address it had; give it
            // that one if it is ours and free.
            const uint32_t server = server_addr(state->netif);
            if (slot < 0 && requested > server && requested <= server + POOL_SIZE) {
                int wanted = (int)(requested - server - 1);
                if (dhcp_leases_claim(&state->leases, wanted, mac)) slot = wanted;
            }

            if (slot >= 0 && requested == slot_addr(state, slot)) {
                reply(state, req, MSG_ACK, requested);
            } else {
                reply(state, req, MSG_NAK, 0);
            }
            break;
        }

        case MSG_RELEASE:
            dhcp_leases_release(&state->leases, slot);
            break;

        default:
            break;
    }
}

static void dhcp_server_task(void *arg) {
    struct dhcp_server_state *state    = arg;
    uint8_t                  *req      = state->request_buf;
    const size_t              req_size = sizeof(state->request_buf);

    while (!state->stop_requested) {
        ssize_t n = lwip_recvfrom(state->fd, req, req_size, 0, NULL, NULL);
        if (n < 0) {
            // Mostly the poll timeout. The pause keeps a socket that is gone
            // from becoming a busy loop; packets wait in lwIP meanwhile.
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // BOOTREQUEST with the DHCP magic cookie.
        if (n >= OPTIONS_OFFSET + 3 && req[0] == 1 && req[BOOTP_FIXED_LEN] == 0x63 &&
            req[BOOTP_FIXED_LEN + 1] == 0x82 && req[BOOTP_FIXED_LEN + 2] == 0x53 && req[BOOTP_FIXED_LEN + 3] == 0x63) {
            on_message(state, req, (uint16_t)n);
        }
    }

    // Last use of the state: the stopper frees it as soon as it has this.
    xSemaphoreGive(state->stop_requested);
    vTaskDelete(NULL);
}

// ---- Start and stop --------------------------------------------------------

// A socket that sees only this interface: a broadcast that arrives on another one
// is not the server's to answer.
static int open_socket(struct netif *netif) {
    int fd = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;

    struct ifreq ifr = {0};
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%c%c%u", netif->name[0], netif->name[1], (unsigned)netif->num);

    const int            broadcast = 1;
    const struct timeval timeout   = {.tv_sec = 0, .tv_usec = POLL_MS * 1000};
    struct sockaddr_in   any       = {
                .sin_len    = sizeof(any),
                .sin_family = AF_INET,
                .sin_port   = lwip_htons(SERVER_PORT),
                .sin_addr   = {.s_addr = PP_HTONL(INADDR_ANY)},
                .sin_zero   = {0},
    };

    if (lwip_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0 ||
        lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        lwip_setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, &ifr, sizeof(ifr)) < 0 ||
        lwip_bind(fd, (struct sockaddr *)&any, sizeof(any)) < 0) {
        lwip_close(fd);
        return -1;
    }
    return fd;
}

static void destroy_state(struct dhcp_server_state *state) {
    if (!state) return;

    if (state->fd >= 0) {
        lwip_close(state->fd);
        state->fd = -1;
    }

    if (state->stop_requested) vSemaphoreDelete(state->stop_requested);

    vPortFree(state);
}

static struct dhcp_server_state *create_state(struct netif *netif) {
    struct dhcp_server_state *state = pvPortMalloc(sizeof(*state));
    if (!state) {
        LOG_W("no memory for the DHCP state");
        return NULL;
    }

    memset(state, 0, sizeof(*state));
    state->netif = netif;
    state->fd    = open_socket(netif);

    if (state->fd < 0) {
        LOG_W("could not listen on port %d of %c%c%u", SERVER_PORT, netif->name[0], netif->name[1],
              (unsigned)netif->num);
        destroy_state(state);
        return NULL;
    }

    return state;
}

bool dhcp_server_start(struct netif *netif) {
    if (s_state && s_state->stop_requested) dhcp_server_stop();
    if (s_state) return s_state->stop_requested == NULL;

    s_state = create_state(netif);
    if (!s_state) return false;

    // Below the tcpip thread: a lease is not urgent.
    // clang-format off
    const BaseType_t created = xTaskCreate(
        dhcp_server_task,
        "dhcpd",
        configMINIMAL_STACK_SIZE * 6,
        s_state,
        tskIDLE_PRIORITY + 2,
        NULL
    );
    // clang-format on

    if (created != pdPASS) {
        LOG_W("no memory for the DHCP task");
        destroy_state(s_state);
        s_state = NULL;
        return false;
    }

    return true;
}

void dhcp_server_stop(void) {
    if (!s_state) return;

    // The task notices within one poll interval and then deletes itself. If it
    // never says so, the state is left alone rather than freed under it.
    const SemaphoreHandle_t stopped =
        s_state->stop_requested ? s_state->stop_requested : xSemaphoreCreateBinaryStatic(&s_state->stop_storage);
    s_state->stop_requested = stopped;
    if (xSemaphoreTake(stopped, pdMS_TO_TICKS(2 * POLL_MS + 100)) == pdTRUE) {
        destroy_state(s_state);
        s_state = NULL;
    } else {
        LOG_W("the DHCP task did not stop");
    }
}
