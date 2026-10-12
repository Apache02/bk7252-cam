#include "ap_frames.h"

#include <string.h>

#define CAPABILITY         0x0421 // ESS, short preamble, short slot time
#define BEACON_INTERVAL_TU 100

static const uint8_t k_rates[]     = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};
static const uint8_t k_ext_rates[] = {0x30, 0x48, 0x60, 0x6c};
static const uint8_t k_tim[]       = {0, 1, 0, 0}; // count, period, bitmap control, bitmap
static const uint8_t k_bcast[6]    = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// ---- Writing ---------------------------------------------------------------

struct writer {
    uint8_t *p;
    size_t   len;
    size_t   cap;
    bool     ok;
};

static void put(struct writer *w, const void *data, size_t n) {
    if (w->len + n > w->cap) {
        w->ok = false;
        return;
    }
    memcpy(w->p + w->len, data, n);
    w->len += n;
}

static void put8(struct writer *w, uint8_t v) { put(w, &v, 1); }

static void put16(struct writer *w, uint16_t v) {
    uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)};
    put(w, b, 2);
}

static void put_ie(struct writer *w, uint8_t id, const void *data, uint8_t n) {
    put8(w, id);
    put8(w, n);
    put(w, data, n);
}

static struct writer start(uint8_t *buf, size_t cap) { return (struct writer){.p = buf, .cap = cap, .ok = true}; }

static size_t finish(const struct writer *w) { return w->ok ? w->len : 0; }

// First frame-control byte, flags, duration, three addresses, sequence control —
// the last two are filled in by the hardware.
static void put_header(struct writer *w, uint8_t type_subtype, const uint8_t da[6], const struct ap_bss *bss) {
    put8(w, type_subtype);
    put8(w, 0);
    put16(w, 0);
    put(w, da, 6);
    put(w, bss->bssid, 6);
    put(w, bss->bssid, 6);
    put16(w, 0);
}

// What a beacon and a probe response share after the fixed fields. The TIM sits
// between the DS parameter set and the ERP element, and is left out of probe
// responses.
static void put_bss_ies(struct writer *w, const struct ap_bss *bss, bool with_tim, uint16_t *tim_oft) {
    put_ie(w, 0, bss->ssid, bss->ssid_len);
    put_ie(w, 1, k_rates, sizeof(k_rates));
    put_ie(w, 3, &bss->channel, 1);
    if (with_tim) {
        if (tim_oft) *tim_oft = (uint16_t)w->len;
        put_ie(w, 5, k_tim, sizeof(k_tim));
    }
    const uint8_t erp = 0;
    put_ie(w, 42, &erp, 1);
    put_ie(w, 50, k_ext_rates, sizeof(k_ext_rates));
}

static void put_bss_fixed(struct writer *w) {
    const uint8_t timestamp[8] = {0};
    put(w, timestamp, sizeof(timestamp));
    put16(w, BEACON_INTERVAL_TU);
    put16(w, CAPABILITY);
}

// ---- Frames ----------------------------------------------------------------

size_t ap_build_beacon(const struct ap_bss *bss, uint8_t *buf, size_t cap, uint16_t *tim_oft, uint8_t *tim_len) {
    struct writer w = start(buf, cap);

    put_header(&w, 0x80, k_bcast, bss);
    put_bss_fixed(&w);
    put_bss_ies(&w, bss, true, tim_oft);

    if (tim_len) *tim_len = 2 + sizeof(k_tim);
    return finish(&w);
}

size_t ap_build_probe_resp(const struct ap_bss *bss, const uint8_t da[6], uint8_t *buf, size_t cap) {
    struct writer w = start(buf, cap);

    put_header(&w, 0x50, da, bss);
    put_bss_fixed(&w);
    put_bss_ies(&w, bss, false, NULL);
    return finish(&w);
}

size_t ap_build_auth_resp(const struct ap_bss *bss, const uint8_t da[6], uint16_t alg, uint16_t status, uint8_t *buf,
                          size_t cap) {
    struct writer w = start(buf, cap);

    put_header(&w, 0xB0, da, bss);
    put16(&w, alg);
    put16(&w, 2); // sequence number of the response
    put16(&w, status);
    return finish(&w);
}

size_t ap_build_assoc_resp(const struct ap_bss *bss, const uint8_t da[6], bool reassoc, uint16_t status, uint16_t aid,
                           uint8_t *buf, size_t cap) {
    struct writer w = start(buf, cap);

    put_header(&w, reassoc ? 0x30 : 0x10, da, bss);
    put16(&w, CAPABILITY);
    put16(&w, status);
    put16(&w, aid | 0xC000); // the two top bits are always set in an AID field
    put_ie(&w, 1, k_rates, sizeof(k_rates));
    put_ie(&w, 50, k_ext_rates, sizeof(k_ext_rates));
    return finish(&w);
}

size_t ap_build_deauth(const struct ap_bss *bss, const uint8_t da[6], uint16_t reason, uint8_t *buf, size_t cap) {
    struct writer w = start(buf, cap);

    put_header(&w, 0xC0, da, bss);
    put16(&w, reason);
    return finish(&w);
}

// ---- Parsing ---------------------------------------------------------------

#define HEADER_LEN 24

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Walks the elements of a probe request, keeping the first SSID.
static bool parse_probe_ies(const uint8_t *f, size_t len, struct ap_rx *rx) {
    size_t i = HEADER_LEN;
    while (i < len) {
        if (i + 2 > len) return false;
        uint8_t id = f[i];
        uint8_t n  = f[i + 1];
        if (i + 2 + n > len) return false;

        if (id == 0 && !rx->has_ssid) {
            if (n > AP_SSID_MAX) return false;
            rx->has_ssid = true;
            rx->ssid     = f + i + 2;
            rx->ssid_len = n;
        }
        i += 2 + (size_t)n;
    }
    return true;
}

bool ap_parse(const uint8_t *f, size_t len, struct ap_rx *rx) {
    memset(rx, 0, sizeof(*rx));

    // Protocol version 0 and type management: both live in the low nibble.
    if (len < HEADER_LEN || (f[0] & 0x0F) != 0) return false;

    memcpy(rx->da, f + 4, 6);
    memcpy(rx->sa, f + 10, 6);
    memcpy(rx->bssid, f + 16, 6);

    switch (f[0] & 0xF0) {
        case 0x40:
            rx->kind = AP_FRAME_PROBE_REQ;
            return parse_probe_ies(f, len, rx);
        case 0xB0:
            if (len < HEADER_LEN + 6) return false;
            rx->kind     = AP_FRAME_AUTH;
            rx->auth_alg = rd16(f + HEADER_LEN);
            rx->auth_seq = rd16(f + HEADER_LEN + 2);
            return true;
        case 0x00: // capability, listen interval
            if (len < HEADER_LEN + 4) return false;
            rx->kind = AP_FRAME_ASSOC_REQ;
            return true;
        case 0x20: // capability, listen interval, current AP
            if (len < HEADER_LEN + 10) return false;
            rx->kind = AP_FRAME_REASSOC_REQ;
            return true;
        case 0xA0:
            if (len < HEADER_LEN + 2) return false;
            rx->kind = AP_FRAME_DISASSOC;
            return true;
        case 0xC0:
            if (len < HEADER_LEN + 2) return false;
            rx->kind = AP_FRAME_DEAUTH;
            return true;
        default:
            return false;
    }
}

bool ap_probe_wanted(const struct ap_bss *bss, const struct ap_rx *rx) {
    if (!rx->has_ssid || rx->ssid_len == 0) return true;
    return rx->ssid_len == bss->ssid_len && memcmp(rx->ssid, bss->ssid, bss->ssid_len) == 0;
}
