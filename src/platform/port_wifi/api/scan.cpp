// One scan at a time, from request to resolved promise.
//
// Three tasks touch this file: the caller starts a scan and later blocks in the
// promise, the kmsg task feeds indications in and reports the sweep's end, and
// the timer task cancels a sweep that runs past its budget. All of it meets in
// the session globals below, behind one mutex.
//
// Parsing follows the vendor's mhdr_scanu_result_ind() (the reference build,
// func/rwnx_intf/rw_msg_rx.c).

#include <new>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <FreeRTOS.h>
#include <semphr.h>
#include <timers.h>

#include "libip/message.h"
#include "wifi/core.h"
#include "wifi/handlers/scan_handler.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"
#include "wifi/scan.h"
#include "wifi/station_status.h"

#define DEBUG_NAME "scan"
#include "debug.h"

// ---- 802.11 management frame layout (ip/mac/mac_frame.h) -------------------

#define MAC_SHORT_MAC_HDR_LEN        24
#define MAC_ADDR3_OFT                16 // BSSID in a beacon / probe response
#define MAC_BEACON_INTERVAL_OFT      (MAC_SHORT_MAC_HDR_LEN + 8)
#define MAC_BEACON_CAPA_OFT          (MAC_SHORT_MAC_HDR_LEN + 10)
#define MAC_BEACON_VARIABLE_PART_OFT (MAC_SHORT_MAC_HDR_LEN + 12)
#define MAC_ELTID_SSID               0
#define MAC_ELTID_DS                 3
#define MAC_ELTID_RSN                48
#define MAC_ELTID_VENDOR             221

#define MAC_CAPA_PRIVACY (1u << 4)

// A halfword read straight off the frame: the payload is 4-aligned and both
// offsets are even, so these stay halfword-aligned as ARMv5 requires.
static uint16_t read_le16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8)); }

// Information elements are a flat run of `id, len, payload[len]`.
static const uint8_t *find_ie(const uint8_t *ies, uint16_t ies_len, uint8_t id) {
    uint16_t offset = 0;
    while (offset + 2 <= ies_len) {
        uint8_t elt_id  = ies[offset];
        uint8_t elt_len = ies[offset + 1];
        if (offset + 2 + elt_len > ies_len) break; // truncated element
        if (elt_id == id) return &ies[offset];
        offset += 2 + elt_len;
    }
    return nullptr;
}

// RSN and the WPA vendor IE both carry an AKM suite list; the last suite tells
// SAE (WPA3) apart from PSK/802.1X (WPA2). Layout: version(2), group cipher(4),
// pairwise count(2) + N*4, akm count(2) + N*4.
static StationSecurity rsn_security(const uint8_t *ie, uint8_t ie_len) {
    if (ie_len < 8) return StationSecurity::Wpa2; // too short to inspect; RSN at minimum

    uint16_t offset = 2 + 4; // version + group cipher suite
    if (offset + 2 > ie_len) return StationSecurity::Wpa2;
    uint16_t pairwise = read_le16(&ie[offset]);
    offset += 2 + pairwise * 4;

    if (offset + 2 > ie_len) return StationSecurity::Wpa2;
    uint16_t akm_cnt = read_le16(&ie[offset]);
    offset += 2;

    for (uint16_t i = 0; i < akm_cnt && offset + 4 <= ie_len; i++, offset += 4) {
        // 00-0F-AC:8 = SAE, 00-0F-AC:9 = FT-SAE — both mean WPA3.
        if (ie[offset] == 0x00 && ie[offset + 1] == 0x0F && ie[offset + 2] == 0xAC &&
            (ie[offset + 3] == 8 || ie[offset + 3] == 9)) {
            return StationSecurity::Wpa3;
        }
    }
    return StationSecurity::Wpa2;
}

static StationSecurity parse_security(const uint8_t *ies, uint16_t ies_len, uint16_t capability) {
    const uint8_t *rsn = find_ie(ies, ies_len, MAC_ELTID_RSN);
    if (rsn) return rsn_security(&rsn[2], rsn[1]);

    // WPA1 rides in a vendor-specific IE: OUI 00-50-F2, type 1.
    static constexpr uint8_t wpa_oui[4] = {0x00, 0x50, 0xF2, 0x01};
    uint16_t                 offset     = 0;
    while (offset + 2 <= ies_len) {
        uint8_t id  = ies[offset];
        uint8_t len = ies[offset + 1];
        if (offset + 2 + len > ies_len) break;
        if (id == MAC_ELTID_VENDOR && len >= 4 && memcmp(&ies[offset + 2], wpa_oui, 4) == 0) {
            return StationSecurity::Wpa;
        }
        offset += 2 + len;
    }

    // No key-management IE, but the privacy bit set means static WEP.
    return (capability & MAC_CAPA_PRIVACY) ? StationSecurity::Wep : StationSecurity::Open;
}

// ---- ScanResult ------------------------------------------------------------

ScanResult::~ScanResult() {
    for (ScanStation *s = stations; s;) {
        ScanStation *next = s->next;
        delete s;
        s = next;
    }
    for (ChannelSurvey *c = survey; c;) {
        ChannelSurvey *next = c->next;
        delete c;
        c = next;
    }
}

// ---- Session ---------------------------------------------------------------

// How long the LMAC gets to wind the sweep down after being asked to, on top of
// the dwells it needs to notice. scan_cancel_req_handler() only raises a flag
// the sweep reads at a channel boundary, and reaching one costs a dwell or two.
#define SCAN_CANCEL_DWELLS   4
#define SCAN_CANCEL_SLACK_MS 500

static SemaphoreHandle_t           s_lock;
static TimerHandle_t               s_timer;
static Shared<Promise<ScanResult>> s_promise;
static ScanResult                 *s_result;
static bool                        s_collect_survey;
static bool                        s_cancelled;
static uint32_t                    s_channel_time_ms;

static void session_finish(ScanStatus status);

// Asks the LMAC's scan task to stop where it is. Nothing waits on the reply:
// the sweep ends through the same SCANU_START_CFM any finished sweep uses.
static int cancel_sweep(void) {
    void *req = ke_msg_alloc(SCAN_CANCEL_REQ, TASK_SCAN, TASK_API, 0);
    if (!req) return -1;
    return rw_msg_post(req);
}

// Fires twice at most: the first expiry cancels the sweep rather than the
// session, because resolving the promise while the LMAC scans on leaves it in
// SCANU_SCANNING, where later requests are saved instead of run. The second
// expiry gives up on the cancel as well.
static void timer_expired(__unused TimerHandle_t timer) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool     live   = !s_promise.empty();
    bool     cancel = live && !s_cancelled;
    uint32_t grace  = s_channel_time_ms * SCAN_CANCEL_DWELLS + SCAN_CANCEL_SLACK_MS;
    if (cancel) s_cancelled = true;
    xSemaphoreGive(s_lock);

    // Already resolved by an indication that beat the timer.
    if (!live) return;

    if (cancel && cancel_sweep() == 0) {
        LOG_W("%s(): over budget, cancelling the sweep", __func__);
        xTimerChangePeriod(s_timer, pdMS_TO_TICKS(grace), 0);
        xTimerStart(s_timer, 0);
        return;
    }

    LOG_W("%s(): the sweep did not stop, giving up on it", __func__);
    session_finish(ScanStatus::Timeout);
}

// First scan only, with nothing in flight, so no indication races the build.
static bool session_init() {
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;

    // Reused rather than per-scan, which would mean deleting a timer from inside
    // its own callback.
    if (!s_timer) {
        s_timer = xTimerCreate("scan", pdMS_TO_TICKS(1000), pdFALSE, nullptr, timer_expired);
    }
    return s_timer != nullptr;
}

// Clears the session under the lock, then resolves outside it. Whoever arrives
// second finds it empty, so exactly one resolve happens.
static void session_finish(ScanStatus status) {
    if (!s_lock) return;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    Shared<Promise<ScanResult>> promise   = s_promise.take();
    ScanResult                 *result    = s_result;
    bool                        cancelled = s_cancelled;
    s_result                              = nullptr;
    s_cancelled                           = false;
    xSemaphoreGive(s_lock);

    if (promise.empty()) return;

    xTimerStop(s_timer, 0);

    // A sweep that stopped because we asked it to is not a finished one, even
    // when the LMAC reports it the same way.
    if (cancelled && status == ScanStatus::Ok) status = ScanStatus::Timeout;

    if (result) result->status = (result->dropped && status == ScanStatus::Ok) ? ScanStatus::Partial : status;

    if (!promise->resolve(result)) delete result;
}

void scan_finish_handler(void) { session_finish(ScanStatus::Ok); }

void scan_survey_add_handler(const void *param, uint16_t param_len) {
    if (!s_lock || param_len < sizeof(struct mm_channel_survey_ind)) return;

    const struct mm_channel_survey_ind *ind = static_cast<const struct mm_channel_survey_ind *>(param);

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_result && s_collect_survey) {
        ChannelSurvey *c = new (std::nothrow) ChannelSurvey;
        if (c) {
            c->freq      = ind->freq;
            c->noise_dbm = ind->noise_dbm;
            c->time_ms   = ind->chan_time_ms;
            c->busy_ms   = ind->chan_time_busy_ms;
            c->next      = nullptr;

            // Appended, so the table comes out in sweep order.
            ChannelSurvey **tail = &s_result->survey;
            while (*tail) tail = &(*tail)->next;
            *tail = c;
        } else {
            s_result->dropped++;
        }
    }
    xSemaphoreGive(s_lock);
}

void scan_ind_add_handler(const void *param, uint16_t param_len) {
    if (!s_lock || param_len < sizeof(struct scanu_result_ind)) return;

    const struct scanu_result_ind *ind   = static_cast<const struct scanu_result_ind *>(param);
    const uint8_t                 *frame = reinterpret_cast<const uint8_t *>(ind->payload);

    // Trust whichever length is smaller: the frame length the LMAC reports, or
    // what actually arrived in the message.
    uint16_t avail = param_len - static_cast<uint16_t>(sizeof(*ind));
    uint16_t len   = ind->length < avail ? ind->length : avail;
    if (len < MAC_BEACON_VARIABLE_PART_OFT) return;

    const uint8_t *ies     = frame + MAC_BEACON_VARIABLE_PART_OFT;
    uint16_t       ies_len = len - MAC_BEACON_VARIABLE_PART_OFT;

    // Channel: prefer the DS Parameter Set element, since a beacon can be heard
    // on an adjacent channel; fall back to the frequency it was received on.
    uint8_t        channel = 0;
    const uint8_t *ds      = find_ie(ies, ies_len, MAC_ELTID_DS);
    if (ds && ds[1] >= 1) {
        channel = ds[2];
    } else if (ind->center_freq == 2484) {
        channel = 14;
    } else if (ind->center_freq >= 2412) {
        channel = static_cast<uint8_t>((ind->center_freq - 2407) / 5);
    }

    const uint8_t *bssid = frame + MAC_ADDR3_OFT;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (!s_result) {
        xSemaphoreGive(s_lock);
        return;
    }

    s_result->indications++;

    // Already known? Keep the strongest sighting.
    for (ScanStation *s = s_result->stations; s; s = s->next) {
        if (memcmp(s->bssid, bssid, 6) == 0) {
            if (ind->rssi > s->rssi) {
                s->rssi    = ind->rssi;
                s->channel = channel;
            }
            xSemaphoreGive(s_lock);
            return;
        }
    }

    ScanStation *s = new (std::nothrow) ScanStation;
    if (!s) {
        s_result->dropped++;
        xSemaphoreGive(s_lock);
        return;
    }

    memcpy(s->bssid, bssid, 6);
    s->channel    = channel;
    s->rssi       = ind->rssi;
    s->beacon_int = read_le16(frame + MAC_BEACON_INTERVAL_OFT);
    s->security   = parse_security(ies, ies_len, read_le16(frame + MAC_BEACON_CAPA_OFT));
    s->ssid_len   = 0;
    s->next       = nullptr;

    const uint8_t *ssid = find_ie(ies, ies_len, MAC_ELTID_SSID);
    if (ssid) {
        uint8_t ssid_len = ssid[1] > SCAN_SSID_MAX ? SCAN_SSID_MAX : ssid[1];
        memcpy(s->ssid, &ssid[2], ssid_len);
        s->ssid_len = ssid_len;
    }

    ScanStation **tail = &s_result->stations;
    while (*tail) tail = &(*tail)->next;
    *tail = s;
    s_result->station_count++;

    xSemaphoreGive(s_lock);
}

// ---- Building the request --------------------------------------------------

// Returns how many slots it filled, so callers can accumulate ssid_cnt.
static uint8_t fill_ssid(struct mac_ssid *slot, const char *ssid) {
    if (!ssid || !*ssid) return 0;

    size_t len = strlen(ssid);
    if (len > sizeof(slot->array)) len = sizeof(slot->array);
    slot->length = static_cast<uint8_t>(len);
    memcpy(slot->array, ssid, len);
    return 1;
}

// "No filter" has to be spelled as an address with the group bit set — see
// ScanConfig. A zeroed config means "any", not "match 00:00:00:00:00:00".
static void fill_bssid(struct mac_addr *dst, const uint8_t bssid[6]) {
    bool any = true;
    for (int i = 0; i < 6; i++) {
        if (bssid[i]) any = false;
    }

    if (any) {
        memset(dst, 0xFF, sizeof(*dst));
    } else {
        memcpy(dst, bssid, 6);
    }
}

// Fire-and-forget by nature: the outcome arrives as indications, and the
// unmatched SCANU_START_CFM is what ends the sweep.
static int rw_msg_send_scan_req(uint8_t vif_idx, const ScanConfig &cfg, uint8_t channel_count) {
    struct scanu_start_req *req = static_cast<struct scanu_start_req *>(
        ke_msg_alloc(SCANU_START_REQ, TASK_SCANU, TASK_API, sizeof(struct scanu_start_req)));
    if (!req) return -1;

    uint8_t first = (channel_count == 1) ? cfg.channel : 1;
    for (uint8_t i = 0; i < channel_count; i++) {
        req->chan[i].freq  = rw_ieee80211_get_centre_frequency(first + i);
        req->chan[i].band  = 0; // 2.4 GHz
        req->chan[i].flags = cfg.longer_channel_time ? SCAN_PASSIVE_BIT : 0;
        // tx_power stays 0, as in the vendor's own builder.
    }
    req->chan_cnt = channel_count;

    fill_bssid(&req->bssid, cfg.bssid);

    // Whichever is present lands in slot 0, so the LMAC never sees a hole.
    uint8_t slots = fill_ssid(&req->ssid[0], cfg.ssid);
    slots += fill_ssid(&req->ssid[slots], cfg.ssid_alt);
    req->ssid_cnt = slots;

    req->vif_idx = vif_idx;
    req->no_cck  = false;

    LOG_I("SCANU_START_REQ vif=%u chan=%u chan_time=%ums ssid=%u", vif_idx, req->chan_cnt,
          cfg.longer_channel_time ? SCAN_CHANNEL_TIME_LONG_MS : SCAN_CHANNEL_TIME_SHORT_MS, req->ssid_cnt);

    return rw_msg_post(req);
}

// ---- wifi_scan -------------------------------------------------------------

Shared<Promise<ScanResult>> wifi_scan(const ScanConfig *config) {
    constexpr ScanConfig defaults = {};
    const ScanConfig    &cfg      = config ? *config : defaults;

    auto iface = wifi_net_ensure_iface(WIFI_IFACE_STA);
    if (!iface) {
        LOG_E("%s(): the station interface could not be brought up", __func__);
        return {};
    }

    if (!session_init()) {
        LOG_E("%s(): session primitives unavailable", __func__);
        return {};
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool busy = !s_promise.empty();
    xSemaphoreGive(s_lock);
    if (busy) {
        LOG_W("%s(): a scan is already in flight", __func__);
        return {};
    }

    Owned<ScanResult> result(new (std::nothrow) ScanResult{});
    if (result.empty()) {
        LOG_W("%s(): OOM for the result", __func__);
        return {};
    }
    result->status = ScanStatus::Ok;

    Shared<Promise<ScanResult>> promise(new (std::nothrow) Promise<ScanResult>());
    if (promise.empty() || !promise->valid()) {
        LOG_W("%s(): OOM for the promise", __func__);
        return {};
    }

    uint32_t channel_time_ms = cfg.longer_channel_time ? SCAN_CHANNEL_TIME_LONG_MS : SCAN_CHANNEL_TIME_SHORT_MS;

    // Published first: an indication can arrive while rw_msg_send_scan_req() is
    // still running. The session takes a reference to the promise and keeps it
    // until it has something to resolve with, so the caller walking away early
    // cannot pull it out from under the handlers. The result changes owner
    // outright — from this frame to the session, and from the session to the
    // promise once the sweep ends.
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_promise         = promise;
    s_result          = result.release();
    s_collect_survey  = cfg.collect_survey;
    s_cancelled       = false;
    s_channel_time_ms = channel_time_ms;
    xSemaphoreGive(s_lock);

    uint8_t channel_count = (cfg.channel >= 1 && cfg.channel <= SCAN_CHANNEL_2G4) ? 1 : SCAN_CHANNEL_2G4;
    // Twice the sweep plus a second, derived from what was actually asked for.
    // Doubled again while associated, because the LMAC steps back to the
    // operating channel after every scanned one, so each channel costs two dwells.
    uint32_t dwells   = mhdr_get_station_status() >= MSG_CONN_SUCCESS ? 4u : 2u;
    uint32_t limit_ms = channel_count * channel_time_ms * dwells + 1000u;
    xTimerChangePeriod(s_timer, pdMS_TO_TICKS(limit_ms), 0);
    xTimerStart(s_timer, 0);

    if (rw_msg_send_scan_req(iface->vif, cfg, channel_count) != 0) {
        LOG_W("%s(): request could not be queued", __func__);
        xTimerStop(s_timer, 0);
        xSemaphoreTake(s_lock, portMAX_DELAY);
        // Empty if session_finish() got here first and gave the result to the
        // promise, which then frees it instead.
        Owned<ScanResult> unsent(s_result);
        s_promise.clear();
        s_result = nullptr;
        xSemaphoreGive(s_lock);
        return {};
    }

    return promise;
}
