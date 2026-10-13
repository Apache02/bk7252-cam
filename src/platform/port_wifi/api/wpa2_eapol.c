#include "wpa2_eapol.h"

#include <string.h>

#include "wpa2_sha1.h"

const uint8_t WPA2_RSN_IE[22] = {0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00,
                                 0x0F, 0xAC, 0x04, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00};

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

bool wpa2_eapol_parse(const uint8_t *frame, size_t len, struct wpa2_eapol_key *out) {
    if (len < WPA2_EAPOL_KEY_FIXED) return false;
    if (frame[1] != 3) return false; // not EAPOL-Key
    if (frame[4] != 2) return false; // not the RSN key descriptor

    size_t total = WPA2_EAPOL_HEADER_LEN + be16(frame + 2);
    if (total < WPA2_EAPOL_KEY_FIXED || total > len) return false;

    uint16_t kdl = be16(frame + 97);
    if (WPA2_EAPOL_KEY_FIXED + (size_t)kdl > total) return false;

    out->key_info     = be16(frame + 5);
    out->key_length   = be16(frame + 7);
    memcpy(out->replay, frame + 9, 8);
    out->nonce        = frame + 17;
    out->key_data     = frame + WPA2_EAPOL_KEY_FIXED;
    out->key_data_len = kdl;
    out->frame_len    = total;
    return true;
}

size_t wpa2_eapol_build(uint8_t *out, size_t out_size, uint16_t key_info, uint16_t key_length,
                        const uint8_t replay[8], const uint8_t nonce[32], const uint8_t *key_data,
                        uint16_t key_data_len, const uint8_t kck[16]) {
    size_t total = WPA2_EAPOL_KEY_FIXED + key_data_len;
    if (total > out_size) return 0;

    memset(out, 0, total);
    out[0] = 1; // 802.1X-2001
    out[1] = 3; // EAPOL-Key
    out[2] = (uint8_t)((total - WPA2_EAPOL_HEADER_LEN) >> 8);
    out[3] = (uint8_t)(total - WPA2_EAPOL_HEADER_LEN);
    out[4] = 2; // RSN key descriptor
    out[5] = (uint8_t)(key_info >> 8);
    out[6] = (uint8_t)key_info;
    out[7] = (uint8_t)(key_length >> 8);
    out[8] = (uint8_t)key_length;
    memcpy(out + 9, replay, 8);
    memcpy(out + 17, nonce, 32);
    out[97] = (uint8_t)(key_data_len >> 8);
    out[98] = (uint8_t)key_data_len;
    if (key_data_len) memcpy(out + WPA2_EAPOL_KEY_FIXED, key_data, key_data_len);

    if (kck) {
        uint8_t mic[WPA2_SHA1_LEN];
        wpa2_hmac_sha1(kck, 16, out, total, mic);
        memcpy(out + WPA2_EAPOL_MIC_OFFSET, mic, WPA2_EAPOL_MIC_LEN);
    }
    return total;
}

bool wpa2_eapol_mic_ok(const uint8_t kck[16], const uint8_t *frame, size_t frame_len) {
    if (frame_len < WPA2_EAPOL_KEY_FIXED) return false;

    static const uint8_t zero[WPA2_EAPOL_MIC_LEN] = {0};

    struct wpa2_hmac_sha1 h;
    wpa2_hmac_sha1_begin(&h, kck, 16);
    wpa2_hmac_sha1_update(&h, frame, WPA2_EAPOL_MIC_OFFSET);
    wpa2_hmac_sha1_update(&h, zero, sizeof(zero));
    wpa2_hmac_sha1_update(&h, frame + WPA2_EAPOL_MIC_OFFSET + WPA2_EAPOL_MIC_LEN,
                          frame_len - WPA2_EAPOL_MIC_OFFSET - WPA2_EAPOL_MIC_LEN);
    uint8_t mic[WPA2_SHA1_LEN];
    wpa2_hmac_sha1_final(&h, mic);

    // No early exit: how long the compare takes should not say how many bytes matched.
    uint8_t diff = 0;
    for (int i = 0; i < WPA2_EAPOL_MIC_LEN; i++) diff |= (uint8_t)(mic[i] ^ frame[WPA2_EAPOL_MIC_OFFSET + i]);
    return diff == 0;
}

bool wpa2_find_gtk(const uint8_t *key_data, size_t len, uint8_t *key_id, uint8_t gtk[16]) {
    static const uint8_t kde[4] = {0x00, 0x0F, 0xAC, 0x01};

    for (size_t i = 0; i + 2 <= len;) {
        uint8_t type = key_data[i];
        uint8_t elen = key_data[i + 1];
        if (type == 0xDD && elen == 0) return false; // the padding that follows the last element
        if (i + 2 + elen > len) return false;

        // OUI and type (4), key id and tx bit (1), reserved (1), then the key.
        if (type == 0xDD && elen == 22 && memcmp(key_data + i + 2, kde, 4) == 0) {
            *key_id = key_data[i + 6] & 0x03;
            memcpy(gtk, key_data + i + 8, 16);
            return true;
        }
        i += 2 + (size_t)elen;
    }
    return false;
}
