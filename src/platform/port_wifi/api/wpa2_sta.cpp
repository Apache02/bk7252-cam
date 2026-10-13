// The WPA2-PSK supplicant: one task, one queue, one handshake at a time.
//
// wpa2_sm.c knows the protocol and nothing about the radio; this file is its
// ops. EAPOL frames arrive on the Wi-Fi core task, which may not block, so they
// are copied into a message and queued. Installing a key and opening the port
// are requests to the LMAC that wait for a confirmation, so they run here, on
// the supplicant's own task.

#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>
#include <queue.h>
#include <task.h>

#include "hardware/random.h"
#include "libip/vif.h"
#include "wifi/connect.h"
#include "wifi/handlers/connect_handler.h"
#include "wifi/handlers/wpa2_handler.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"

#include "wpa2_sm.h"

// #define DEBUG_NAME "wpa2"
#include "debug.h"

enum class Kind : uint8_t { Arm, Eapol, Disarm, Abort };

struct Msg {
    Kind     kind;
    uint8_t  vif;
    uint16_t len;
    uint8_t  data[1]; // Arm: the PMK, then the BSSID. Eapol: the Ethernet frame.
};

#define QUEUE_DEPTH 8

static QueueHandle_t s_queue;

// Owned by the task from here down.
static struct wpa2_sm s_sm;
static bool           s_active;  // frames are being handled; stays set after the join, for group rekeys
static bool           s_pending; // the connect session still waits for the verdict
static uint8_t        s_vif;
static uint8_t        s_aa[6];
static uint8_t        s_own[6];

// The LMAC's key slots of the installed keys, kept to free them: a key left in
// the LMAC is still used by the next association with the same AP index.
#define NO_SLOT 0xFF
static uint8_t s_ptk_slot = NO_SLOT;
static uint8_t s_gtk_slot = NO_SLOT;

static void drop_keys() {
    if (s_ptk_slot != NO_SLOT) rw_msg_send_mm_key_del(s_ptk_slot);
    if (s_gtk_slot != NO_SLOT) rw_msg_send_mm_key_del(s_gtk_slot);
    s_ptk_slot = NO_SLOT;
    s_gtk_slot = NO_SLOT;
}

// ---- ops -------------------------------------------------------------------

static uint8_t ap_index() { return vif_info_tab[s_vif].u.sta.ap_id; }

static bool op_send(__unused void *ctx, const uint8_t *eapol, size_t len) {
    uint8_t frame[14 + WPA2_EAPOL_MAX];
    if (len > WPA2_EAPOL_MAX) return false;

    memcpy(frame, s_aa, 6);
    memcpy(frame + 6, s_own, 6);
    frame[12] = 0x88;
    frame[13] = 0x8E;
    memcpy(frame + 14, eapol, len);
    return wifi_net_send_raw(s_vif, frame, (uint16_t)(14 + len)) == ERR_OK;
}

static bool op_install_ptk(__unused void *ctx, const uint8_t tk[16]) {
    return rw_msg_send_mm_key_add(s_vif, ap_index(), 0, tk, &s_ptk_slot) == 0;
}

static bool op_install_gtk(__unused void *ctx, uint8_t key_id, const uint8_t gtk[16]) {
    uint8_t old_slot = s_gtk_slot;
    uint8_t slot     = NO_SLOT;
    if (rw_msg_send_mm_key_add(s_vif, 0xFF, key_id, gtk, &slot) != 0) return false;

    // A rekey brings a new key; the one it replaces is freed unless the LMAC reused its slot.
    if (old_slot != NO_SLOT && old_slot != slot) rw_msg_send_mm_key_del(old_slot);
    s_gtk_slot = slot;
    return true;
}

static bool op_open_port(__unused void *ctx) { return rw_msg_send_me_set_control_port(ap_index(), true) == 0; }

static void op_random(__unused void *ctx, uint8_t *out, size_t len) {
    trng_enable();
    get_random_bytes(out, len);
}

static const struct wpa2_ops k_ops = {nullptr, op_send, op_install_ptk, op_install_gtk, op_open_port, op_random};

// ---- the task --------------------------------------------------------------

// Leaves the BSS so the interface is free for another attempt, then tells the session.
static void fail() {
    s_active  = false;
    s_pending = false;
    LOG_W("handshake failed");
    wifi_disconnect(s_vif);
    wpa2_result_handler(false);
}

static void handle(const Msg *m) {
    switch (m->kind) {
        case Kind::Arm: {
            const struct net_if *iface = wifi_net_iface(WIFI_IFACE_STA);
            if (!iface) return;

            drop_keys();
            s_vif = m->vif;
            memcpy(s_aa, m->data + 32, 6);
            memcpy(s_own, iface->netif.hwaddr, 6);
            wpa2_sm_init(&s_sm, &k_ops, m->data, s_aa, s_own);
            s_active  = true;
            s_pending = true;
            break;
        }

        case Kind::Eapol: {
            // Ethernet header (14), then the 802.1X header (4) at least.
            if (!s_active || m->len < 14 + 4) return;
            if (memcmp(m->data + 6, s_aa, 6) != 0) return; // not from our AP

            enum wpa2_result r = wpa2_sm_rx(&s_sm, m->data + 14, m->len - 14);
            if (r == WPA2_DONE) {
                s_pending = false;
                wpa2_result_handler(true);
            } else if (r == WPA2_FAILED) {
                fail();
            }
            break;
        }

        case Kind::Disarm:
            s_active  = false;
            s_pending = false;
            drop_keys();
            break;

        case Kind::Abort:
            // The session's timer can fire a moment before the verdict lands; an
            // abort that arrives after it must not take down a link that is up.
            if (s_active && s_pending) fail();
            break;
    }
}

static void wpa2_task(__unused void *arg) {
    for (;;) {
        Msg *m = nullptr;
        if (xQueueReceive(s_queue, &m, portMAX_DELAY) != pdTRUE || !m) continue;
        handle(m);
        vPortFree(m);
    }
}

// Starts the task and its queue on first use, from a caller that may block.
static bool ensure_task() {
    if (s_queue) return true;

    s_queue = xQueueCreate(QUEUE_DEPTH, sizeof(Msg *));
    if (!s_queue) return false;

    if (xTaskCreate(wpa2_task, "wifi_wpa2", 1024, nullptr, configMAX_PRIORITIES - 4, nullptr) != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = nullptr;
        return false;
    }
    return true;
}

static Msg *make(Kind kind, uint8_t vif, const uint8_t *data, uint16_t len) {
    Msg *m = static_cast<Msg *>(pvPortMalloc(sizeof(Msg) + len));
    if (!m) return nullptr;

    m->kind = kind;
    m->vif  = vif;
    m->len  = len;
    if (len) memcpy(m->data, data, len);
    return m;
}

// timeout 0 from the core task and the timer, which must not wait.
static bool post(Msg *m, TickType_t timeout) {
    if (!m) return false;
    if (xQueueSend(s_queue, &m, timeout) == pdTRUE) return true;

    vPortFree(m);
    return false;
}

// ---- entry points ----------------------------------------------------------

bool wpa2_sta_arm(uint8_t vif_idx, const uint8_t pmk[32], const uint8_t bssid[6]) {
    if (!ensure_task()) return false;

    uint8_t data[32 + 6];
    memcpy(data, pmk, 32);
    memcpy(data + 32, bssid, 6);
    return post(make(Kind::Arm, vif_idx, data, sizeof(data)), pdMS_TO_TICKS(100));
}

void wpa2_sta_disarm(void) {
    if (s_queue) post(make(Kind::Disarm, 0, nullptr, 0), 0);
}

void wpa2_sta_abort(void) {
    if (s_queue) post(make(Kind::Abort, 0, nullptr, 0), 0);
}

void wpa2_eapol_rx(uint8_t vif_idx, const uint8_t *frame, uint16_t len) {
    if (!s_queue || len > 14 + WPA2_EAPOL_MAX) return;
    post(make(Kind::Eapol, vif_idx, frame, len), 0);
}
