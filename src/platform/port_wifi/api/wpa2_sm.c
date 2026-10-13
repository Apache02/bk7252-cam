#include "wpa2_sm.h"

#include <string.h>

#include "wpa2_aes.h"
#include "wpa2_eapol.h"
#include "wpa2_kdf.h"

// Decrypted key data of message 3 or group message 1 is an RSN IE and a GTK KDE
// with padding: far below this.
#define KEY_DATA_MAX 192

void wpa2_sm_init(struct wpa2_sm *sm, const struct wpa2_ops *ops, const uint8_t pmk[32], const uint8_t aa[6],
                  const uint8_t spa[6]) {
    memset(sm, 0, sizeof(*sm));
    sm->ops = *ops;
    memcpy(sm->pmk, pmk, 32);
    memcpy(sm->aa, aa, 6);
    memcpy(sm->spa, spa, 6);
    sm->state = WPA2_IDLE;
}

static bool send_key(struct wpa2_sm *sm, uint16_t key_info, const uint8_t replay[8], const uint8_t *nonce,
                     const uint8_t *key_data, uint16_t key_data_len) {
    static const uint8_t zero[32] = {0};

    uint8_t buf[WPA2_EAPOL_MAX];
    size_t  n = wpa2_eapol_build(buf, sizeof(buf), key_info, 0, replay, nonce ? nonce : zero, key_data, key_data_len,
                                 sm->ptk);
    return n && sm->ops.send(sm->ops.ctx, buf, n);
}

// The counter must grow from one authenticated message to the next.
static bool replay_ok(const struct wpa2_sm *sm, const uint8_t replay[8]) {
    return !sm->have_replay || memcmp(replay, sm->replay, 8) > 0;
}

static void note_replay(struct wpa2_sm *sm, const uint8_t replay[8]) {
    memcpy(sm->replay, replay, 8);
    sm->have_replay = true;
}

// Unwraps the key data and returns the GTK in it.
static bool unwrap_gtk(const uint8_t kek[16], const struct wpa2_eapol_key *k, uint8_t gtk[16], uint8_t *id) {
    if (!(k->key_info & KEY_INFO_ENCRYPTED)) return false;
    if (k->key_data_len < 24 || k->key_data_len % 8 || (size_t)k->key_data_len - 8 > KEY_DATA_MAX) return false;

    uint8_t plain[KEY_DATA_MAX];
    if (!wpa2_aes_key_unwrap(kek, k->key_data, k->key_data_len, plain)) return false;
    return wpa2_find_gtk(plain, (size_t)k->key_data_len - 8, id, gtk);
}

// A GTK that is already installed is not installed again: the AP repeats group
// messages, and a second install of the same key has to be harmless.
static bool install_gtk_once(struct wpa2_sm *sm, uint8_t id, const uint8_t gtk[16]) {
    if (sm->gtk_installed && sm->gtk_id == id && memcmp(sm->gtk, gtk, 16) == 0) return true;
    if (!sm->ops.install_gtk(sm->ops.ctx, id, gtk)) return false;

    memcpy(sm->gtk, gtk, 16);
    sm->gtk_id        = id;
    sm->gtk_installed = true;
    return true;
}

static enum wpa2_result on_msg1(struct wpa2_sm *sm, const struct wpa2_eapol_key *k) {
    // A message 1 once the keys are in would start a PTK rekey, which is not supported.
    if (sm->ptk_installed) return WPA2_IGNORED;

    // The AP repeats message 1 when it has not heard message 2 yet. The answer has to
    // carry the same SNonce, or the AP, which may have built its PTK from the first
    // one, and this side end up with different keys.
    if (sm->state != WPA2_GOT_MSG1 || memcmp(sm->anonce, k->nonce, 32) != 0) {
        memcpy(sm->anonce, k->nonce, 32);
        sm->ops.random(sm->ops.ctx, sm->snonce, sizeof(sm->snonce));
        wpa2_ptk(sm->pmk, sm->aa, sm->spa, sm->anonce, sm->snonce, sm->ptk);
    }

    // Message 1 has no MIC, so its counter is not taken as the baseline: a forged one
    // could otherwise lock out every real message 3.
    if (!send_key(sm, KEY_INFO_MSG2, k->replay, sm->snonce, WPA2_RSN_IE, sizeof(WPA2_RSN_IE))) return WPA2_FAILED;

    sm->state = WPA2_GOT_MSG1;
    return WPA2_PROGRESS;
}

static enum wpa2_result on_msg3(struct wpa2_sm *sm, const struct wpa2_eapol_key *k, const uint8_t *frame) {
    if (sm->state == WPA2_IDLE) return WPA2_IGNORED;

    if (!wpa2_eapol_mic_ok(sm->ptk, frame, k->frame_len)) return WPA2_IGNORED;
    if (!replay_ok(sm, k->replay)) return WPA2_IGNORED;
    if (memcmp(k->nonce, sm->anonce, 32) != 0 || k->key_length != 16) return WPA2_IGNORED;

    uint8_t gtk[16], gtk_id;
    if (!unwrap_gtk(sm->ptk + 16, k, gtk, &gtk_id)) return WPA2_FAILED;

    note_replay(sm, k->replay);

    // Message 4 goes out before the key is installed: the AP expects it in the clear.
    if (!send_key(sm, KEY_INFO_MSG4, k->replay, NULL, NULL, 0)) return WPA2_FAILED;

    if (!sm->ptk_installed) {
        if (!sm->ops.install_ptk(sm->ops.ctx, sm->ptk + 32)) return WPA2_FAILED;
        sm->ptk_installed = true;
    }
    if (!install_gtk_once(sm, gtk_id, gtk)) return WPA2_FAILED;

    if (sm->state == WPA2_ESTABLISHED) return WPA2_PROGRESS; // a repeated message 3

    if (!sm->ops.open_port(sm->ops.ctx)) return WPA2_FAILED;
    sm->state = WPA2_ESTABLISHED;
    return WPA2_DONE;
}

static enum wpa2_result on_group1(struct wpa2_sm *sm, const struct wpa2_eapol_key *k, const uint8_t *frame) {
    if (sm->state != WPA2_ESTABLISHED) return WPA2_IGNORED;

    if (!wpa2_eapol_mic_ok(sm->ptk, frame, k->frame_len)) return WPA2_IGNORED;
    if (!replay_ok(sm, k->replay)) return WPA2_IGNORED;

    uint8_t gtk[16], gtk_id;
    if (!unwrap_gtk(sm->ptk + 16, k, gtk, &gtk_id)) return WPA2_FAILED;

    note_replay(sm, k->replay);

    if (!send_key(sm, KEY_INFO_GROUP_MSG2, k->replay, NULL, NULL, 0)) return WPA2_FAILED;
    if (!install_gtk_once(sm, gtk_id, gtk)) return WPA2_FAILED;
    return WPA2_PROGRESS;
}

enum wpa2_result wpa2_sm_rx(struct wpa2_sm *sm, const uint8_t *frame, size_t len) {
    struct wpa2_eapol_key k;
    if (len > WPA2_EAPOL_MAX || !wpa2_eapol_parse(frame, len, &k)) return WPA2_IGNORED;

    // The AP asks, the station answers: only frames that carry the ACK bit are for us, and only in
    // the format that goes with CCMP and the PSK key management we offered.
    if (!(k.key_info & KEY_INFO_ACK) || (k.key_info & KEY_INFO_VERSION_MASK) != 2) return WPA2_IGNORED;

    if (k.key_info & KEY_INFO_PAIRWISE) {
        if (!(k.key_info & KEY_INFO_MIC) && !(k.key_info & KEY_INFO_INSTALL)) return on_msg1(sm, &k);
        if ((k.key_info & KEY_INFO_MIC) && (k.key_info & KEY_INFO_INSTALL)) return on_msg3(sm, &k, frame);
        return WPA2_IGNORED;
    }

    if ((k.key_info & KEY_INFO_MIC) && (k.key_info & KEY_INFO_SECURE)) return on_group1(sm, &k, frame);
    return WPA2_IGNORED;
}
