#include "platform/telnet_server.h"

#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>

#include "lwip/sockets.h"

#include "utils/ring_buffer.h"

// #define DEBUG_NAME "telnetd"
#include "debug.h"

#define DEFAULT_STACK_WORDS (configMINIMAL_STACK_SIZE * 6)

// Same level as the HTTP workers, so the console stays usable while a stream runs.
#define TASK_PRIORITY (configMAX_PRIORITIES - 3)

// A second client waits here, connected but unanswered, until the first one leaves.
#define LISTEN_BACKLOG 1

// How long the session waits for input before it looks at its output queue and at
// whether the server was stopped; also the worst delay before output reaches the client.
#define POLL_MS 20

// How long the task waits in accept() before looking at whether it was stopped.
#define ACCEPT_POLL_MS 200

#define IO_TIMEOUT_S 5

// How long a write waits for room in the output queue before it drops the rest. A client
// that keeps up frees the room within milliseconds; one that does not must not hold up
// the writers, which include the Wi-Fi tasks' logging.
#define WRITE_WAIT_MS 100

// A client that has been silent this long is probed, and dropped when it does not answer.
#define KEEPALIVE_IDLE_S     30
#define KEEPALIVE_INTERVAL_S 5
#define KEEPALIVE_COUNT      3

// Powers of two, as ring_buffer wants.
#define TX_QUEUE_SIZE 1024
#define RX_QUEUE_SIZE 128
#define CHUNK         128

static_assert((TX_QUEUE_SIZE & (TX_QUEUE_SIZE - 1)) == 0 && (RX_QUEUE_SIZE & (RX_QUEUE_SIZE - 1)) == 0,
              "queue sizes must be powers of two");

// Telnet commands and the two options this server asks for.
#define IAC  255
#define DONT 254
#define DO   253
#define WONT 252
#define WILL 251
#define SB   250
#define SE   240

#define OPT_ECHO 1
#define OPT_SGA  3

// Each queue has one consumer: the session task for the output, whoever calls
// telnet_server_read() for the input. The input has one producer, the session task;
// the output has several, so those writes run in a critical section.
struct telnet_server {
    telnet_server_config_t config;
    int                    listen_fd;
    volatile bool          stopping;
    volatile bool          disconnect; // the attached client is to be dropped

    SemaphoreHandle_t exited; // given by the task as its last act
    TaskHandle_t      task;

    bool           active; // a client is attached; output is queued only then
    struct ringbuf tx;
    uint8_t        tx_buf[TX_QUEUE_SIZE];
    struct ringbuf rx;
    uint8_t        rx_buf[RX_QUEUE_SIZE];
};

// One server at a time. Whoever reads it from another task does so in a critical section
// and finishes there: stop() clears it in one before the server is freed.
static struct telnet_server *s_server;

// ---- Queues ----------------------------------------------------------------

void telnet_server_write(const void *data, size_t len) {
    if (xPortIsInsideInterrupt()) return;

    const uint8_t   *bytes = data;
    const TickType_t start = xTaskGetTickCount();

    for (;;) {
        bool can_wait = false;

        taskENTER_CRITICAL();
        struct telnet_server *server = s_server;
        if (server && server->active) {
            while (len > 0 && ringbuf_put_byte(&server->tx, *bytes)) {
                bytes++;
                len--;
            }
            // The server task is the one that makes room, so it cannot wait for itself.
            can_wait = xTaskGetCurrentTaskHandle() != server->task;
        } else {
            len = 0;
        }
        taskEXIT_CRITICAL();

        if (len == 0 || !can_wait || xTaskGetTickCount() - start >= pdMS_TO_TICKS(WRITE_WAIT_MS)) return;
        vTaskDelay(1);
    }
}

// The critical section is for the server's lifetime, not for the queue: stop() frees it.
int telnet_server_read(void) {
    taskENTER_CRITICAL();
    struct telnet_server *server = s_server;
    const int             c      = server ? ringbuf_get_char(&server->rx) : -1;
    taskEXIT_CRITICAL();

    return c;
}

bool telnet_server_disconnect(void) {
    taskENTER_CRITICAL();
    struct telnet_server *server   = s_server;
    const bool            attached = server && server->active;
    if (attached) server->disconnect = true;
    taskEXIT_CRITICAL();

    return attached;
}

static void set_active(struct telnet_server *server, bool active) {
    // Nothing is queued while inactive, so whatever the last client left can go.
    uint8_t b;
    if (active) {
        while (ringbuf_get_byte(&server->tx, &b)) {
        }
    }

    taskENTER_CRITICAL();
    server->active = active;
    taskEXIT_CRITICAL();
}

// ---- Connection ------------------------------------------------------------

static bool send_all(int fd, const void *data, size_t len) {
    return lwip_send(fd, data, len, 0) == (ssize_t)len;
}

// Sends the queued output, with 0xFF doubled as Telnet wants it.
static bool flush_output(struct telnet_server *server, int fd) {
    uint8_t raw[CHUNK];
    uint8_t out[2 * CHUNK];

    size_t n = 0;
    while (n < sizeof(raw) && ringbuf_get_byte(&server->tx, &raw[n])) n++;
    if (n == 0) return true;

    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        out[used++] = raw[i];
        if (raw[i] == IAC) out[used++] = IAC;
    }
    return send_all(fd, out, used);
}

enum { STATE_DATA, STATE_IAC, STATE_OPTION, STATE_SUBNEGOTIATION, STATE_SUBNEGOTIATION_IAC };

struct parser {
    uint8_t state;
    uint8_t command; // DO, DONT, WILL or WONT while STATE_OPTION
    bool    after_cr;
};

// The client answers ECHO and SGA, which are the only options the server asks for, and
// those answers need no reply. Anything else it offers or asks for is declined.
static bool decline(int fd, uint8_t command, uint8_t option) {
    if (option == OPT_ECHO || option == OPT_SGA) return true;
    if (command != DO && command != WILL) return true;

    const uint8_t reply[] = {IAC, command == DO ? WONT : DONT, option};
    return send_all(fd, reply, sizeof(reply));
}

// Strips the Telnet framing from what the client sent and queues the plain bytes.
// Returns false when a reply could not be sent.
static bool parse(struct telnet_server *server, struct parser *p, int fd, uint8_t b) {
    switch (p->state) {
        case STATE_DATA:
            if (b == IAC) {
                p->state = STATE_IAC;
                break;
            }
            // Enter arrives as CR LF or CR NUL; the console should see one CR.
            if (p->after_cr && (b == 0 || b == '\n')) {
                p->after_cr = false;
                break;
            }
            p->after_cr = (b == '\r');
            ringbuf_put_byte(&server->rx, b);
            break;

        case STATE_IAC:
            if (b == IAC) {
                ringbuf_put_byte(&server->rx, IAC);
                p->state = STATE_DATA;
            } else if (b >= WILL && b <= DONT) {
                p->command = b;
                p->state   = STATE_OPTION;
            } else if (b == SB) {
                p->state = STATE_SUBNEGOTIATION;
            } else {
                p->state = STATE_DATA;
            }
            break;

        case STATE_OPTION:
            p->state = STATE_DATA;
            return decline(fd, p->command, b);

        case STATE_SUBNEGOTIATION:
            if (b == IAC) p->state = STATE_SUBNEGOTIATION_IAC;
            break;

        case STATE_SUBNEGOTIATION_IAC:
            p->state = (b == SE) ? STATE_DATA : STATE_SUBNEGOTIATION;
            break;
    }
    return true;
}

static void configure_socket(int fd) {
    const struct timeval timeout  = {.tv_sec = IO_TIMEOUT_S, .tv_usec = 0};
    const int            on       = 1;
    const int            idle     = KEEPALIVE_IDLE_S;
    const int            interval = KEEPALIVE_INTERVAL_S;
    const int            count    = KEEPALIVE_COUNT;

    // Only the keepalive matters for correctness; a console works without the rest.
    if (lwip_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ||
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0 ||
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0 ||
        lwip_setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0) {
        LOG_W("keepalive is not available");
    }
    lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static void serve(struct telnet_server *server, int fd) {
    configure_socket(fd);

    // Character-at-a-time mode: the server echoes (the shell does it) and the client
    // neither echoes nor waits for go-ahead.
    static const uint8_t NEGOTIATION[] = {IAC, WILL, OPT_ECHO, IAC, WILL, OPT_SGA, IAC, DO, OPT_SGA};
    if (!send_all(fd, NEGOTIATION, sizeof(NEGOTIATION))) return;

    const char *greeting = server->config.greeting;
    if (greeting && !send_all(fd, greeting, strlen(greeting))) return;

    server->disconnect = false;
    set_active(server, true);
    if (server->config.on_connect) server->config.on_connect(server->config.user);

    struct parser parser = {.state = STATE_DATA};
    while (!server->stopping && !server->disconnect) {
        // Output waiting means no time to wait for input.
        struct timeval wait = {.tv_sec = 0, .tv_usec = ringbuf_is_empty(&server->tx) ? POLL_MS * 1000 : 0};
        fd_set         readable;
        FD_ZERO(&readable);
        FD_SET(fd, &readable);

        const int ready = lwip_select(fd + 1, &readable, NULL, NULL, &wait);
        if (ready < 0) break;

        if (ready > 0) {
            uint8_t   in[CHUNK];
            const int n = lwip_recv(fd, in, sizeof(in), 0);
            if (n <= 0) break;

            bool ok = true;
            for (int i = 0; ok && i < n; i++) ok = parse(server, &parser, fd, in[i]);
            if (!ok) break;
        }

        if (!flush_output(server, fd)) break;
    }

    // Whatever the client was still owed goes out before the connection closes.
    set_active(server, false);
    while (!ringbuf_is_empty(&server->tx) && flush_output(server, fd)) {
    }

    if (server->config.on_disconnect) server->config.on_disconnect(server->config.user);
}

static void telnet_task(void *arg) {
    struct telnet_server *server = arg;

    while (!server->stopping) {
        const int fd = lwip_accept(server->listen_fd, NULL, NULL);
        if (fd < 0) {
            // Mostly the poll timeout. The pause keeps a dead socket from
            // becoming a busy loop; connections wait in lwIP meanwhile.
            vTaskDelay(1);
            continue;
        }

        serve(server, fd);
        lwip_close(fd);
    }

    // Last use of the server: the stopper frees it as soon as it has this.
    xSemaphoreGive(server->exited);
    vTaskDelete(NULL);
}

// ---- Start and stop --------------------------------------------------------

static int open_listen_socket(uint16_t port) {
    const int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;

    const int            reuse   = 1;
    const struct timeval timeout = {.tv_sec = 0, .tv_usec = ACCEPT_POLL_MS * 1000};
    struct sockaddr_in   any     = {
              .sin_len    = sizeof(any),
              .sin_family = AF_INET,
              .sin_port   = lwip_htons(port),
              .sin_addr   = {.s_addr = PP_HTONL(INADDR_ANY)},
              .sin_zero   = {0},
    };

    if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        lwip_bind(fd, (struct sockaddr *)&any, sizeof(any)) < 0 || lwip_listen(fd, LISTEN_BACKLOG) < 0) {
        lwip_close(fd);
        return -1;
    }
    return fd;
}

static void destroy_server(struct telnet_server *server) {
    if (server->listen_fd >= 0) lwip_close(server->listen_fd);
    if (server->exited) vSemaphoreDelete(server->exited);
    vPortFree(server);
}

bool telnet_server_start(const telnet_server_config_t *config) {
    if (!config || s_server) return false;

    struct telnet_server *server = pvPortMalloc(sizeof(*server));
    if (!server) {
        LOG_W("no memory for the Telnet server");
        return false;
    }

    memset(server, 0, sizeof(*server));
    server->config = *config;
    if (server->config.stack_words == 0) server->config.stack_words = DEFAULT_STACK_WORDS;
    server->tx        = (struct ringbuf){.data = server->tx_buf, .mask = TX_QUEUE_SIZE - 1};
    server->rx        = (struct ringbuf){.data = server->rx_buf, .mask = RX_QUEUE_SIZE - 1};
    server->exited    = xSemaphoreCreateBinary();
    server->listen_fd = open_listen_socket(config->port);

    if (!server->exited || server->listen_fd < 0) {
        LOG_W("could not listen on port %u", (unsigned)config->port);
        destroy_server(server);
        return false;
    }

    if (xTaskCreate(telnet_task, "telnet", server->config.stack_words, server, TASK_PRIORITY, &server->task) != pdPASS) {
        LOG_W("no memory for the Telnet task");
        destroy_server(server);
        return false;
    }

    taskENTER_CRITICAL();
    s_server = server;
    taskEXIT_CRITICAL();
    return true;
}

void telnet_server_stop(void) {
    struct telnet_server *server = s_server;
    if (!server) return;

    // The task sees the flag within one poll interval and detaches its client on the way
    // out, so from then on nothing is queued. If it never reports, the server stays
    // allocated rather than being freed under it.
    server->stopping = true;
    if (xSemaphoreTake(server->exited, pdMS_TO_TICKS((IO_TIMEOUT_S + 2) * 1000)) != pdTRUE) {
        LOG_W("the Telnet task did not stop");
        return;
    }

    // A call already inside the critical section finishes before the server goes.
    taskENTER_CRITICAL();
    s_server = NULL;
    taskEXIT_CRITICAL();

    destroy_server(server);
}

bool telnet_server_is_up(void) {
    return s_server != NULL;
}
