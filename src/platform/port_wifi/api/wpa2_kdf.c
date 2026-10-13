#include "wpa2_kdf.h"

#include <string.h>

#include "wpa2_sha1.h"

bool wpa2_pbkdf2_sha1(const uint8_t *pass, size_t pass_len, const uint8_t *salt, size_t salt_len, unsigned iterations,
                      uint8_t *out, size_t out_len) {
    if (salt_len > 32 || iterations == 0) return false;

    struct wpa2_hmac_sha1 base;
    wpa2_hmac_sha1_begin(&base, pass, pass_len);

    uint8_t first[32 + 4];
    memcpy(first, salt, salt_len);

    for (uint32_t block = 1; out_len > 0; block++) {
        first[salt_len]     = (uint8_t)(block >> 24);
        first[salt_len + 1] = (uint8_t)(block >> 16);
        first[salt_len + 2] = (uint8_t)(block >> 8);
        first[salt_len + 3] = (uint8_t)block;

        uint8_t u[WPA2_SHA1_LEN], t[WPA2_SHA1_LEN];
        struct wpa2_hmac_sha1 h = base;
        wpa2_hmac_sha1_update(&h, first, salt_len + 4);
        wpa2_hmac_sha1_final(&h, u);
        memcpy(t, u, sizeof(t));

        for (unsigned i = 1; i < iterations; i++) {
            h = base;
            wpa2_hmac_sha1_update(&h, u, sizeof(u));
            wpa2_hmac_sha1_final(&h, u);
            for (int j = 0; j < WPA2_SHA1_LEN; j++) t[j] ^= u[j];
        }

        size_t n = out_len < sizeof(t) ? out_len : sizeof(t);
        memcpy(out, t, n);
        out += n;
        out_len -= n;
    }
    return true;
}

bool wpa2_pmk(const char *passphrase, const uint8_t *ssid, size_t ssid_len, uint8_t pmk[WPA2_PMK_LEN]) {
    size_t n = passphrase ? strlen(passphrase) : 0;
    if (n < 8 || n > 63 || ssid_len < 1 || ssid_len > 32) return false;
    for (size_t i = 0; i < n; i++) {
        if ((uint8_t)passphrase[i] < 32 || (uint8_t)passphrase[i] > 126) return false;
    }
    return wpa2_pbkdf2_sha1((const uint8_t *)passphrase, n, ssid, ssid_len, 4096, pmk, WPA2_PMK_LEN);
}

bool wpa2_prf(const uint8_t *key, size_t key_len, const char *label, const uint8_t *data, size_t data_len,
              uint8_t *out, size_t out_len) {
    uint8_t buf[128];
    size_t  label_len = strlen(label);
    if (label_len + 1 + data_len + 1 > sizeof(buf)) return false;

    memcpy(buf, label, label_len);
    buf[label_len] = 0;
    memcpy(buf + label_len + 1, data, data_len);
    size_t n = label_len + 1 + data_len;

    struct wpa2_hmac_sha1 base;
    wpa2_hmac_sha1_begin(&base, key, key_len);

    for (uint8_t counter = 0; out_len > 0; counter++) {
        buf[n] = counter;

        uint8_t d[WPA2_SHA1_LEN];
        struct wpa2_hmac_sha1 h = base;
        wpa2_hmac_sha1_update(&h, buf, n + 1);
        wpa2_hmac_sha1_final(&h, d);

        size_t c = out_len < sizeof(d) ? out_len : sizeof(d);
        memcpy(out, d, c);
        out += c;
        out_len -= c;
    }
    return true;
}

void wpa2_ptk(const uint8_t pmk[WPA2_PMK_LEN], const uint8_t aa[6], const uint8_t spa[6], const uint8_t anonce[32],
              const uint8_t snonce[32], uint8_t ptk[WPA2_PTK_LEN]) {
    uint8_t data[6 + 6 + 32 + 32];

    const uint8_t *lo_a = memcmp(aa, spa, 6) < 0 ? aa : spa;
    const uint8_t *hi_a = lo_a == aa ? spa : aa;
    const uint8_t *lo_n = memcmp(anonce, snonce, 32) < 0 ? anonce : snonce;
    const uint8_t *hi_n = lo_n == anonce ? snonce : anonce;

    memcpy(data, lo_a, 6);
    memcpy(data + 6, hi_a, 6);
    memcpy(data + 12, lo_n, 32);
    memcpy(data + 44, hi_n, 32);

    // Cannot fail: the label and the 76 bytes fit the buffer.
    (void)wpa2_prf(pmk, WPA2_PMK_LEN, "Pairwise key expansion", data, sizeof(data), ptk, WPA2_PTK_LEN);
}
