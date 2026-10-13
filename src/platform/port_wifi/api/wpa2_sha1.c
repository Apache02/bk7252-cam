#include "wpa2_sha1.h"

#include <string.h>

static uint32_t rol(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static void compress(uint32_t h[5], const uint8_t b[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)b[4 * i] << 24) | ((uint32_t)b[4 * i + 1] << 16) | ((uint32_t)b[4 * i + 2] << 8) |
               b[4 * i + 3];
    }
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (bb & c) | (~bb & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = bb ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (bb & c) | (bb & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = bb ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e          = d;
        d          = c;
        c          = rol(bb, 30);
        bb         = a;
        a          = t;
    }
    h[0] += a;
    h[1] += bb;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void wpa2_sha1_init(struct wpa2_sha1 *s) {
    s->h[0] = 0x67452301;
    s->h[1] = 0xEFCDAB89;
    s->h[2] = 0x98BADCFE;
    s->h[3] = 0x10325476;
    s->h[4] = 0xC3D2E1F0;
    s->total = 0;
    s->used  = 0;
}

void wpa2_sha1_update(struct wpa2_sha1 *s, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    s->total += len;
    while (len) {
        size_t n = 64 - s->used;
        if (n > len) n = len;
        memcpy(s->block + s->used, p, n);
        s->used += (uint32_t)n;
        p += n;
        len -= n;
        if (s->used == 64) {
            compress(s->h, s->block);
            s->used = 0;
        }
    }
}

void wpa2_sha1_final(struct wpa2_sha1 *s, uint8_t out[WPA2_SHA1_LEN]) {
    uint64_t bits = s->total * 8;

    uint8_t pad = 0x80;
    wpa2_sha1_update(s, &pad, 1);
    pad = 0;
    while (s->used != 56) wpa2_sha1_update(s, &pad, 1);

    uint8_t len[8];
    for (int i = 0; i < 8; i++) len[i] = (uint8_t)(bits >> (56 - 8 * i));
    wpa2_sha1_update(s, len, 8);

    for (int i = 0; i < 5; i++) {
        out[4 * i]     = (uint8_t)(s->h[i] >> 24);
        out[4 * i + 1] = (uint8_t)(s->h[i] >> 16);
        out[4 * i + 2] = (uint8_t)(s->h[i] >> 8);
        out[4 * i + 3] = (uint8_t)s->h[i];
    }
}

void wpa2_sha1(const uint8_t *data, size_t len, uint8_t out[WPA2_SHA1_LEN]) {
    struct wpa2_sha1 s;
    wpa2_sha1_init(&s);
    wpa2_sha1_update(&s, data, len);
    wpa2_sha1_final(&s, out);
}

void wpa2_hmac_sha1_begin(struct wpa2_hmac_sha1 *h, const uint8_t *key, size_t key_len) {
    uint8_t k[64] = {0};
    if (key_len > sizeof(k)) {
        wpa2_sha1(key, key_len, k);
    } else {
        memcpy(k, key, key_len);
    }

    uint8_t pad[64];
    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x36;
    wpa2_sha1_init(&h->inner);
    wpa2_sha1_update(&h->inner, pad, sizeof(pad));

    for (int i = 0; i < 64; i++) pad[i] = k[i] ^ 0x5c;
    wpa2_sha1_init(&h->outer);
    wpa2_sha1_update(&h->outer, pad, sizeof(pad));
}

void wpa2_hmac_sha1_update(struct wpa2_hmac_sha1 *h, const void *data, size_t len) {
    wpa2_sha1_update(&h->inner, data, len);
}

void wpa2_hmac_sha1_final(struct wpa2_hmac_sha1 *h, uint8_t out[WPA2_SHA1_LEN]) {
    uint8_t digest[WPA2_SHA1_LEN];
    wpa2_sha1_final(&h->inner, digest);
    wpa2_sha1_update(&h->outer, digest, sizeof(digest));
    wpa2_sha1_final(&h->outer, out);
}

void wpa2_hmac_sha1(const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
                    uint8_t out[WPA2_SHA1_LEN]) {
    struct wpa2_hmac_sha1 h;
    wpa2_hmac_sha1_begin(&h, key, key_len);
    wpa2_hmac_sha1_update(&h, data, data_len);
    wpa2_hmac_sha1_final(&h, out);
}
