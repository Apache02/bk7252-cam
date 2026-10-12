#pragma once

// The 802.11 management frames an open access point exchanges, as pure functions
// over byte buffers: nothing here talks to the LMAC.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AP_SSID_MAX 32

// Large enough for every frame built here, with a 32-byte SSID.
#define AP_FRAME_MAX 160

struct ap_bss {
    uint8_t bssid[6];
    uint8_t ssid[AP_SSID_MAX];
    uint8_t ssid_len;
    uint8_t channel;
};

enum ap_frame_kind {
    AP_FRAME_OTHER,
    AP_FRAME_PROBE_REQ,
    AP_FRAME_AUTH,
    AP_FRAME_ASSOC_REQ,
    AP_FRAME_REASSOC_REQ,
    AP_FRAME_DISASSOC,
    AP_FRAME_DEAUTH,
};

// What ap_parse() found. ssid points into the parsed frame.
struct ap_rx {
    enum ap_frame_kind kind;
    uint8_t            da[6];
    uint8_t            sa[6];
    uint8_t            bssid[6];
    uint16_t           auth_alg;
    uint16_t           auth_seq;
    bool               has_ssid; // probe requests only
    const uint8_t     *ssid;
    uint8_t            ssid_len;
};

// 802.11 status codes used here.
#define AP_STATUS_OK              0
#define AP_STATUS_UNSUPPORTED_ALG 13
#define AP_STATUS_BAD_SEQUENCE    14
#define AP_STATUS_NO_ROOM         17

// 802.11 reason codes used here.
#define AP_REASON_UNSPECIFIED 1
#define AP_REASON_NOT_AUTHED  6

// Beacon template for APM_START_REQ. *tim_oft is where the TIM element starts and
// *tim_len its whole length.
size_t ap_build_beacon(const struct ap_bss *bss, uint8_t *buf, size_t cap, uint16_t *tim_oft, uint8_t *tim_len);
size_t ap_build_probe_resp(const struct ap_bss *bss, const uint8_t da[6], uint8_t *buf, size_t cap);
size_t ap_build_auth_resp(const struct ap_bss *bss, const uint8_t da[6], uint16_t alg, uint16_t status, uint8_t *buf,
                          size_t cap);
size_t ap_build_assoc_resp(const struct ap_bss *bss, const uint8_t da[6], bool reassoc, uint16_t status, uint16_t aid,
                           uint8_t *buf, size_t cap);
size_t ap_build_deauth(const struct ap_bss *bss, const uint8_t da[6], uint16_t reason, uint8_t *buf, size_t cap);

// Reads a received frame. False for anything that is not a well-formed
// management frame of a kind an AP handles, including any frame cut short or
// carrying an element that runs past its end.
bool ap_parse(const uint8_t *frame, size_t len, struct ap_rx *rx);

// A probe request is answered when it asks for any network or for this one.
bool ap_probe_wanted(const struct ap_bss *bss, const struct ap_rx *rx);

#ifdef __cplusplus
}
#endif
