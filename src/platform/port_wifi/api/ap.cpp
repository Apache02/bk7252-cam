// An open access point.
//
// The LMAC beacons by itself from a template built here, and leaves every other
// management exchange to the host. Its frame hook runs in the core task and may
// not block, while answering a client takes confirmations from that same task. So
// the hook only copies the frame into a queue, and a task of its own does the work.

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

#include "libip/phy.h"
#include "libip/tasks/apm.h"
#include "libip/vif.h"
#include "wifi/ap.h"
#include "wifi/handlers/ap_handler.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"

#include "ap_clients.h"
#include "ap_frames.h"

// #define DEBUG_NAME "ap"
#include "debug.h"

#define AP_ADDRESS 0xC0A80401u // 192.168.4.1
#define AP_NETMASK 0xFFFFFF00u

// Frames waiting for the AP task, and the largest one worth keeping: a request of
// any kind is far smaller.
#define RX_QUEUE_LEN 8
#define RX_FRAME_MAX 512

struct RxFrame {
    uint16_t len;
    uint8_t  vif;
    uint8_t *data; // just past this header
};

// ---- Session ---------------------------------------------------------------

static SemaphoreHandle_t s_lock;    // guards s_clients and s_bcmc_idx
static SemaphoreHandle_t s_session; // held while a frame is handled, and while stopping
static QueueHandle_t     s_queue;
static StaticQueue_t     s_queue_buf;
static uint8_t           s_queue_storage[RX_QUEUE_LEN * sizeof(RxFrame *)];
static StaticTask_t      s_task_buf;
static StackType_t       s_stack[1024];
static bool              s_task_created;

static volatile bool     s_running;
static struct ap_bss     s_bss;
static uint8_t           s_vif      = INVALID_VIF_IDX;
static uint8_t           s_bcmc_idx = AP_NO_STATION;
static struct ap_clients s_clients;

// The LMAC reads the template while the AP runs, so it outlives the call.
static uint8_t s_beacon[AP_FRAME_MAX];

static uint32_t s_rx_dropped;

// ---- Frames from the LMAC --------------------------------------------------

extern "C" void ap_mgmt_rx(const uint8_t *frame, uint16_t len, uint8_t vif_idx) {
    if (!s_running || vif_idx != s_vif) return;

    if (len > RX_FRAME_MAX) {
        s_rx_dropped++;
        return;
    }

    RxFrame *f = static_cast<RxFrame *>(pvPortMalloc(sizeof(RxFrame) + len));
    if (!f) {
        s_rx_dropped++;
        return;
    }

    f->len  = len;
    f->vif  = vif_idx;
    f->data = reinterpret_cast<uint8_t *>(f + 1);
    memcpy(f->data, frame, len);

    if (xQueueSend(s_queue, &f, 0) != pdTRUE) {
        vPortFree(f);
        s_rx_dropped++;
    }
}

extern "C" void ap_assoc_ind_handler(const void *param, uint16_t param_len) {
    if (param_len < 6) return;
    __unused const uint8_t *m = static_cast<const uint8_t *>(param);
    LOG_I("client joined %02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

extern "C" void ap_deassoc_ind_handler(const void *param, uint16_t param_len) {
    if (param_len < 6) return;
    __unused const uint8_t *m = static_cast<const uint8_t *>(param);
    LOG_I("client left %02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

extern "C" uint8_t ap_tx_staid(const uint8_t dest[6]) {
    if (dest[0] & 0x01) return s_bcmc_idx;

    uint8_t idx = AP_NO_STATION;
    if (!s_lock) return idx;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    ap_client *c = ap_clients_find(&s_clients, dest);
    if (c && c->state == AP_CLIENT_ASSOC) idx = c->sta_idx;
    xSemaphoreGive(s_lock);
    return idx;
}

// ---- The AP task -----------------------------------------------------------

// Table access: only the AP task writes, but the data path reads.
static void lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
static void unlock() { xSemaphoreGive(s_lock); }

static void send_frame(uint8_t *frame, size_t len) {
    if (!len) return;
    if (rw_msg_send_mgmt_tx(s_vif, frame, static_cast<uint16_t>(len)) != 0) LOG_W("management frame not sent");
}

// Takes a client out of the LMAC, if it got that far, and frees its slot.
static void drop_client(ap_client *c) {
    uint8_t sta_idx = AP_NO_STA;
    if (c->state == AP_CLIENT_ASSOC) sta_idx = c->sta_idx;

    lock();
    ap_clients_remove(&s_clients, c);
    unlock();

    if (sta_idx != AP_NO_STA) rw_msg_send_me_sta_del(sta_idx);
}

static void on_auth(const ap_rx &rx) {
    uint8_t  out[AP_FRAME_MAX];
    uint16_t status = AP_STATUS_OK;

    if (rx.auth_alg != 0) {
        status = AP_STATUS_UNSUPPORTED_ALG;
    } else if (rx.auth_seq != 1) {
        status = AP_STATUS_BAD_SEQUENCE;
    } else {
        // A client that authenticates again while associated is starting over
        // (a phone reconnecting after a drop): the old station goes first.
        ap_client *c = ap_clients_find(&s_clients, rx.sa);
        if (c) drop_client(c);

        lock();
        c = ap_clients_add(&s_clients, rx.sa);
        unlock();
        if (!c) status = AP_STATUS_NO_ROOM;
    }

    send_frame(out, ap_build_auth_resp(&s_bss, rx.sa, rx.auth_alg, status, out, sizeof(out)));
}

static void on_assoc(const ap_rx &rx, bool reassoc) {
    uint8_t out[AP_FRAME_MAX];

    ap_client *c = ap_clients_find(&s_clients, rx.sa);
    if (!c) {
        send_frame(out, ap_build_deauth(&s_bss, rx.sa, AP_REASON_NOT_AUTHED, out, sizeof(out)));
        return;
    }

    // A second request from a client already registered replaces the first.
    if (c->state == AP_CLIENT_ASSOC) {
        uint8_t old = c->sta_idx;
        lock();
        c->state   = AP_CLIENT_AUTHED;
        c->sta_idx = AP_NO_STA;
        unlock();
        rw_msg_send_me_sta_del(old);
    }

    uint16_t aid = ap_clients_aid(&s_clients, c);
    send_frame(out, ap_build_assoc_resp(&s_bss, rx.sa, reassoc, AP_STATUS_OK, aid, out, sizeof(out)));

    me_sta_add_cfm cfm = {};
    cfm.status         = 0xFF;
    if (rw_msg_send_me_sta_add(s_vif, rx.sa, aid, &cfm) != 0 || cfm.status != 0) {
        LOG_W("the LMAC refused ME_STA_ADD_REQ, status=%u", cfm.status);
        drop_client(c);
        send_frame(out, ap_build_deauth(&s_bss, rx.sa, AP_REASON_UNSPECIFIED, out, sizeof(out)));
        return;
    }

    lock();
    c->sta_idx = cfm.sta_idx;
    c->state   = AP_CLIENT_ASSOC;
    unlock();

    // APM_ASSOC_IND follows this request.
    rw_msg_send_me_set_control_port(cfm.sta_idx, true);
}

static void handle_frame(const uint8_t *frame, uint16_t len) {
    ap_rx rx;
    if (!ap_parse(frame, len, &rx)) return;

    // The LMAC hands back a copy of every frame we send. Answering one would be
    // answering ourselves.
    if (memcmp(rx.sa, s_bss.bssid, 6) == 0) return;

    uint8_t out[AP_FRAME_MAX];

    if (rx.kind == AP_FRAME_PROBE_REQ) {
        if (ap_probe_wanted(&s_bss, &rx)) send_frame(out, ap_build_probe_resp(&s_bss, rx.sa, out, sizeof(out)));
        return;
    }

    // Everything else is addressed to this BSS or not for us.
    if (memcmp(rx.da, s_bss.bssid, 6) != 0) return;

    switch (rx.kind) {
        case AP_FRAME_AUTH:
            on_auth(rx);
            break;
        case AP_FRAME_ASSOC_REQ:
            on_assoc(rx, false);
            break;
        case AP_FRAME_REASSOC_REQ:
            on_assoc(rx, true);
            break;
        case AP_FRAME_DISASSOC:
        case AP_FRAME_DEAUTH: {
            ap_client *c = ap_clients_find(&s_clients, rx.sa);
            if (c) drop_client(c);
            break;
        }
        default:
            break;
    }
}

static void dhcp_server_task(__unused void *arg) {
    for (;;) {
        RxFrame *f = nullptr;
        if (xQueueReceive(s_queue, &f, portMAX_DELAY) != pdTRUE) continue;

        // A frame being answered finishes before wifi_ap_stop() clears the clients,
        // or the client it is adding would be left behind in the LMAC.
        xSemaphoreTake(s_session, portMAX_DELAY);
        if (s_running) handle_frame(f->data, f->len);
        xSemaphoreGive(s_session);
        vPortFree(f);
    }
}

// First start only.
static bool session_init() {
    if (s_task_created) return true;

    s_lock    = xSemaphoreCreateMutex();
    s_session = xSemaphoreCreateMutex();
    if (!s_lock || !s_session) return false;

    s_queue = xQueueCreateStatic(RX_QUEUE_LEN, sizeof(RxFrame *), s_queue_storage, &s_queue_buf);

    // Below the core and kmsg tasks, whose confirmations this one waits for.
    xTaskCreateStatic(dhcp_server_task, "wifi_ap", sizeof(s_stack) / sizeof(s_stack[0]), nullptr, configMAX_PRIORITIES - 4,
                      s_stack, &s_task_buf);

    s_task_created = true;
    return true;
}

// ---- wifi_ap_start / wifi_ap_stop ------------------------------------------

bool wifi_ap_start(const ApConfig *config) {
    if (!config || !config->ssid) return false;

    size_t ssid_len = strlen(config->ssid);
    if (ssid_len == 0 || ssid_len > AP_SSID_MAX) {
        LOG_W("%s(): the SSID must be 1 to %u bytes", __func__, AP_SSID_MAX);
        return false;
    }
    if (config->channel < 1 || config->channel > 14) {
        LOG_W("%s(): channel %u is not 1..14", __func__, config->channel);
        return false;
    }
    if (s_running) {
        LOG_W("%s(): already running", __func__);
        return false;
    }

    struct net_if *iface = wifi_net_ensure_iface(WIFI_IFACE_AP);
    if (!iface) {
        LOG_W("%s(): the AP interface could not be brought up", __func__);
        return false;
    }
    if (!session_init()) {
        LOG_W("%s(): session primitives unavailable", __func__);
        return false;
    }

    memset(&s_bss, 0, sizeof(s_bss));
    memcpy(s_bss.bssid, iface->netif.hwaddr, 6);
    memcpy(s_bss.ssid, config->ssid, ssid_len);
    s_bss.ssid_len = static_cast<uint8_t>(ssid_len);
    s_bss.channel  = config->channel;
    s_vif          = iface->vif;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    ap_clients_clear(&s_clients);
    s_bcmc_idx = AP_NO_STATION;
    xSemaphoreGive(s_lock);

    uint16_t tim_oft    = 0;
    uint8_t  tim_len    = 0;
    size_t   beacon_len = ap_build_beacon(&s_bss, s_beacon, sizeof(s_beacon), &tim_oft, &tim_len);
    if (!beacon_len) return false;

    // Parked first, as for a station: the AP's own channel must be the one the
    // radio is on when beaconing starts.
    if (rw_msg_set_channel(config->channel, PHY_CHNL_BW_20, nullptr) != 0) {
        LOG_W("%s(): could not park on channel %u", __func__, config->channel);
    }

    // Frames can arrive before the confirmation does.
    s_running = true;

    apm_start_cfm cfm = {};
    cfm.status        = 0xFF;
    if (rw_msg_send_apm_start_req(s_vif, config->channel, s_beacon, static_cast<uint16_t>(beacon_len), tim_oft, tim_len,
                                  &cfm) != 0 ||
        cfm.status != 0) {
        LOG_W("%s(): the LMAC refused APM_START_REQ, status=%u", __func__, cfm.status);
        s_running = false;
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_bcmc_idx = cfm.bcmc_idx;
    xSemaphoreGive(s_lock);

    wifi_net_ap_link_up(AP_ADDRESS, AP_NETMASK);

    LOG_I("AP \"%s\" up on channel %u, vif=%u bcmc=%u", config->ssid, config->channel, s_vif, cfm.bcmc_idx);
    return true;
}

bool wifi_ap_stop(void) {
    if (!s_running) return false;

    s_running = false;
    wifi_net_ap_link_down();

    // The LMAC holds a station for each associated client, and a restart would
    // otherwise find them still there. Taken after s_running is down, so a frame
    // already in hand finishes first and nothing new starts.
    xSemaphoreTake(s_session, portMAX_DELAY);
    for (int i = 0; i < AP_CLIENTS_MAX; i++) {
        ap_client *c = &s_clients.slot[i];
        if (c->state != AP_CLIENT_FREE) drop_client(c);
    }
    xSemaphoreGive(s_session);

    if (rw_msg_send_apm_stop_req(s_vif) != 0) {
        LOG_W("%s(): the LMAC refused APM_STOP_REQ", __func__);
        return false;
    }
    return true;
}
