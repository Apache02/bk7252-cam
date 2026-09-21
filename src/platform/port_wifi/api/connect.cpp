// One association at a time, from request to resolved promise.
//
// Three tasks touch this file: the caller starts an association and later blocks
// in the promise, the kmsg task delivers the indication, and the timer task ends
// an attempt that never produced one. All of it meets in the session globals
// below, behind one mutex — the same shape as scan.cpp.
//
// Telling the archive we are connected is the other half of the job, and not an
// optional one: mhdr_get_station_status() gates power-save and traffic detection
// inside libip_7221u.a. See wifi/station_status.h.

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <timers.h>

#include "libip/phy.h"
#include "libip/tasks/sm.h"
#include "wifi/connect.h"
#include "wifi/handlers/connect_handler.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"
#include "wifi/scan.h"
#include "wifi/station_status.h"

#define DEBUG_NAME "connect"
#include "debug.h"

// How long to wait for SM_CONNECT_IND. The BSS is always known by the time the
// request goes out, so this only has to cover the join probe, the authentication
// and the association.
#define CONNECT_LIMIT_MS 10000

// ---- Session ---------------------------------------------------------------

static SemaphoreHandle_t              s_lock;
static TimerHandle_t                  s_timer;
static Shared<Promise<ConnectResult>> s_promise;
static ConnectResult                 *s_result;

static void session_finish(ConnectStatus status);

static void timer_expired(__unused TimerHandle_t timer) {
    LOG_W("%s(): no indication before the deadline", __func__);
    session_finish(ConnectStatus::NoReply);
}

// First association only, with nothing in flight, so no indication races the
// build.
static bool session_init() {
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;

    // Reused rather than per-attempt, which would mean deleting a timer from
    // inside its own callback.
    if (!s_timer) {
        s_timer = xTimerCreate("connect", pdMS_TO_TICKS(CONNECT_LIMIT_MS), pdFALSE, nullptr, timer_expired);
    }
    return s_timer != nullptr;
}

// Clears the session under the lock, then resolves outside it. Whoever arrives
// second finds it empty and does nothing, so exactly one resolve happens.
static void session_finish(ConnectStatus status) {
    if (!s_lock) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    Shared<Promise<ConnectResult>> promise = s_promise.take();
    ConnectResult                 *result  = s_result;
    s_result                               = nullptr;
    xSemaphoreGive(s_lock);

    if (promise.empty()) return;

    xTimerStop(s_timer, 0);

    if (result) result->status = status;

    if (!promise->resolve(result)) delete result;
}

// ---- Indications -----------------------------------------------------------

void sm_connect_ind_handler(const void *param, uint16_t param_len) {
    if (param_len < sizeof(struct sm_connect_indication)) return;

    const struct sm_connect_indication *ind = static_cast<const struct sm_connect_indication *>(param);
    bool                                ok  = ind->status_code == 0;

    // Set before anything else: a caller woken by the promise may go straight on
    // to work that depends on the archive agreeing we are connected. This also
    // runs when nobody is waiting, which is what a roam looks like.
    mhdr_set_station_status(ok ? MSG_CONN_SUCCESS : MSG_CONN_FAIL);

    LOG_I("SM_CONNECT_IND status=%u vif=%u ap=%u ch=%u bssid=%02x:%02x:%02x:%02x:%02x:%02x", ind->status_code,
          ind->vif_idx, ind->ap_idx, ind->ch_idx, ind->bssid[0], ind->bssid[1], ind->bssid[2], ind->bssid[3],
          ind->bssid[4], ind->bssid[5]);

    if (!s_lock) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_result) {
        s_result->status_code = ind->status_code;
        memcpy(s_result->bssid, ind->bssid, sizeof(s_result->bssid));
        s_result->vif_idx = ind->vif_idx;
        s_result->ap_idx  = ind->ap_idx;
        s_result->ch_idx  = ind->ch_idx;
    }
    xSemaphoreGive(s_lock);

    session_finish(ok ? ConnectStatus::Associated : ConnectStatus::Refused);
}

void sm_disconnect_ind_handler(const void *param, uint16_t param_len) {
    __unused uint16_t reason = 0;
    if (param_len >= sizeof(struct sm_disconnect_ind)) {
        reason = static_cast<const struct sm_disconnect_ind *>(param)->reason_code;
    }

    LOG_W("SM_DISCONNECT_IND reason=%u", reason);
    mhdr_set_station_status(MSG_CONN_FAIL);
    wifi_net_link_down();

    // A pending attempt is left alone on purpose: the LMAC drops the old link
    // partway through an association and still sends SM_CONNECT_IND afterward,
    // so resolving here would report a failure the indication then contradicts.
}

// ---- Finding the BSS -------------------------------------------------------

// A directed scan for one SSID, returning the strongest match — several APs can
// share an SSID, so RSSI decides. Blocks for a whole sweep, and repeats it up to
// retry_count times, because a single sweep can miss a network that is there.
static bool find_bss(const char *ssid, uint8_t bssid[6], uint8_t *channel, const int retry_count = 3) {
    ScanConfig cfg = {
        .ssid                = ssid,
        .longer_channel_time = true,
        .collect_survey      = false,
    };

    // An SSID longer than a scan result can hold could never match one.
    size_t len = strlen(ssid);
    if (len > SCAN_SSID_MAX) {
        LOG_W("%s(): \"%s\" is too long to match a scan result", __func__, ssid);
        return false;
    }
    const uint8_t ssid_len = static_cast<uint8_t>(len);

    bool found = false;
    int try_number = 0;
    do {
        Shared<Promise<ScanResult>> promise = wifi_scan(&cfg);
        if (promise.empty()) return false;

        Owned<ScanResult> result = promise->await();
        if (result.empty()) return false;

        // The SSID in the request only shapes the probe requests the LMAC sends;
        // every beacon it hears comes back as a result either way. Matching here
        // is what makes the scan directed.
        const ScanStation *best = nullptr;
        for (const ScanStation *s = result->stations; s; s = s->next) {
            if (s->ssid_len != ssid_len || memcmp(s->ssid, ssid, ssid_len) != 0) continue;
            if (!best || s->rssi > best->rssi) best = s;
        }

        found = best != nullptr;
        if (found) {
            memcpy(bssid, best->bssid, 6);
            *channel = best->channel;
            LOG_I("AP \"%s\" is %02x:%02x:%02x:%02x:%02x:%02x on channel %u, %d dBm", ssid, bssid[0], bssid[1], bssid[2],
                  bssid[3], bssid[4], bssid[5], *channel, best->rssi);
        }
    } while (!found && ++try_number < retry_count);

    return found;
}

// ---- wifi_connect_start ----------------------------------------------------

Shared<Promise<ConnectResult>> wifi_connect_start(uint8_t vif_idx, const ConnectConfig *config) {
    if (!config || !config->ssid || !*config->ssid) {
        LOG_W("%s(): empty SSID", __func__);
        return {};
    }
    if (!session_init()) {
        LOG_W("%s(): session primitives unavailable", __func__);
        return {};
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool busy = !s_promise.empty();
    xSemaphoreGive(s_lock);
    if (busy) {
        LOG_W("%s(): an association is already in flight", __func__);
        return {};
    }

    // Checked before the session is published, so a scan here cannot collide
    // with an association that is already running.
    uint8_t bssid[6];
    uint8_t channel = config->channel;
    memcpy(bssid, config->bssid, sizeof(bssid));

    bool have_bssid = false;
    for (size_t i = 0; i < sizeof(bssid); i++) {
        if (bssid[i]) have_bssid = true;
    }

    if ((!have_bssid || !channel) && !find_bss(config->ssid, bssid, &channel)) {
        LOG_W("%s(): \"%s\" not found", __func__, config->ssid);
        return {};
    }

    // Park the radio on the BSS's channel before asking to join it. SM_CONNECT_REQ
    // opens with a one-channel scan that probes the moment it has retuned, so a
    // join probe issued right after a full sweep is often lost and the
    // association then fails with an unspecified status.
    //
    // The confirmation is waited for: the LMAC sends it only after
    // phy_set_channel() has returned (mm_task.o, mm_hw_config_handler).
    if (rw_msg_set_channel(channel, PHY_CHNL_BW_20, nullptr) != 0) {
        LOG_W("%s(): could not park on channel %u before joining", __func__, channel);
        // Not fatal: the join scan will retune on its own, just without the settle.
    }

    Owned<ConnectResult> result(new (std::nothrow) ConnectResult{});
    if (result.empty()) {
        LOG_W("%s(): OOM for the result", __func__);
        return {};
    }

    Shared<Promise<ConnectResult>> promise(new (std::nothrow) Promise<ConnectResult>());
    if (promise.empty() || !promise->valid()) {
        LOG_W("%s(): OOM for the promise", __func__);
        return {};
    }

    // Published first: the indication can arrive while the request below is
    // still being confirmed. The session takes a reference to the promise and
    // keeps it until it has something to resolve with, so the caller walking
    // away early cannot pull it out from under the handlers. The result changes
    // owner outright — from this frame to the session, and from the session to
    // the promise once the attempt ends.
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_promise = promise;
    s_result  = result.release();
    xSemaphoreGive(s_lock);

    mhdr_set_station_status(MSG_CONNECTING);
    xTimerChangePeriod(s_timer, pdMS_TO_TICKS(CONNECT_LIMIT_MS), 0);
    xTimerStart(s_timer, 0);

    LOG_I("SM_CONNECT_REQ \"%s\" bssid=%02x:%02x:%02x:%02x:%02x:%02x ch=%u", config->ssid, bssid[0], bssid[1],
          bssid[2], bssid[3], bssid[4], bssid[5], channel);

    if (rw_msg_send_sm_connect_req(vif_idx, config->ssid, bssid, channel) != 0) {
        LOG_W("%s(): the LMAC refused the request", __func__);
        xTimerStop(s_timer, 0);
        mhdr_set_station_status(MSG_CONN_FAIL);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        // Empty if session_finish() got here first and gave the result to the
        // promise, which then frees it instead.
        Owned<ConnectResult> unsent(s_result);
        s_promise.clear();
        s_result = nullptr;
        xSemaphoreGive(s_lock);
        return {};
    }

    return promise;
}

// ---- wifi_disconnect -------------------------------------------------------

bool wifi_disconnect(uint8_t vif_idx) {
    // Taken down first: DHCP stops and lwIP drops what it has queued before the
    // deauthentication goes out, rather than after the link is already gone.
    wifi_net_link_down();

    if (rw_msg_send_sm_disconnect_req(vif_idx, MAC_RS_SENDER_LEAVING) != 0) {
        LOG_W("%s(): the LMAC refused the request", __func__);
        return false;
    }

    // SM_DISCONNECT_IND sets this too, but it may never come — the archive sends
    // none when there was no link to lose.
    mhdr_set_station_status(MSG_IDLE);
    return true;
}
