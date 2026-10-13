#pragma once

// SHA-1 and HMAC-SHA1, in software: the hardware SHA block is not used here.
// Pure C, no RTOS, so the host tests compile it as it is.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WPA2_SHA1_LEN 20

struct wpa2_sha1 {
    uint32_t h[5];
    uint64_t total; // bytes fed so far
    uint8_t  block[64];
    uint32_t used;
};

void wpa2_sha1_init(struct wpa2_sha1 *s);
void wpa2_sha1_update(struct wpa2_sha1 *s, const void *data, size_t len);
void wpa2_sha1_final(struct wpa2_sha1 *s, uint8_t out[WPA2_SHA1_LEN]);
void wpa2_sha1(const uint8_t *data, size_t len, uint8_t out[WPA2_SHA1_LEN]);

// The inner and outer states are kept after the key pads, so a caller that runs
// many messages under one key (PBKDF2 runs thousands) copies the struct instead
// of hashing the pads again.
struct wpa2_hmac_sha1 {
    struct wpa2_sha1 inner;
    struct wpa2_sha1 outer;
};

void wpa2_hmac_sha1_begin(struct wpa2_hmac_sha1 *h, const uint8_t *key, size_t key_len);
void wpa2_hmac_sha1_update(struct wpa2_hmac_sha1 *h, const void *data, size_t len);
void wpa2_hmac_sha1_final(struct wpa2_hmac_sha1 *h, uint8_t out[WPA2_SHA1_LEN]);
void wpa2_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                    uint8_t out[WPA2_SHA1_LEN]);

#ifdef __cplusplus
}
#endif
