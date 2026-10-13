#pragma once

// The station's side of the WPA2-PSK handshake, with no idea what radio it runs
// on: frames come in through wpa2_sm_rx(), and everything it wants done goes out
// through the ops. The supplicant task supplies ops that talk to the LMAC; the
// host tests supply ones that record.
//
// Frames start at the 802.1X header, as in wpa2_eapol.h.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The largest EAPOL frame taken or sent. Message 3 with an RSN IE and a GTK is
// about 160 bytes.
#define WPA2_EAPOL_MAX 512

enum wpa2_result {
    WPA2_IGNORED,  // not for us, not valid, or out of order: nothing happened
    WPA2_PROGRESS, // a step was taken, the handshake is not (or no longer news) finished
    WPA2_DONE,     // the keys are in and the port is open; reported once
    WPA2_FAILED,   // an authenticated frame we cannot use, or an op failed
};

struct wpa2_ops {
    void *ctx;
    // One EAPOL frame to send to the AP, the Ethernet header being the caller's.
    bool (*send)(void *ctx, const uint8_t *eapol, size_t len);
    bool (*install_ptk)(void *ctx, const uint8_t tk[16]);
    bool (*install_gtk)(void *ctx, uint8_t key_id, const uint8_t gtk[16]);
    bool (*open_port)(void *ctx);
    // Fills len bytes that must not repeat between handshakes (the SNonce).
    void (*random)(void *ctx, uint8_t *out, size_t len);
};

enum wpa2_state {
    WPA2_IDLE,        // waiting for message 1
    WPA2_GOT_MSG1,    // message 2 sent, waiting for message 3
    WPA2_ESTABLISHED, // keys installed
};

struct wpa2_sm {
    struct wpa2_ops ops;
    uint8_t         pmk[32];
    uint8_t         aa[6];  // the AP
    uint8_t         spa[6]; // us
    uint8_t         ptk[48];
    uint8_t         anonce[32];
    uint8_t         snonce[32];
    uint8_t         replay[8]; // the last counter of a frame that passed its MIC
    bool            have_replay;
    uint8_t         gtk[16];
    uint8_t         gtk_id;
    bool            gtk_installed;
    bool            ptk_installed;
    enum wpa2_state state;
};

void wpa2_sm_init(struct wpa2_sm *sm, const struct wpa2_ops *ops, const uint8_t pmk[32], const uint8_t aa[6],
                  const uint8_t spa[6]);

// One EAPOL frame from the AP. Frames longer than WPA2_EAPOL_MAX are ignored.
enum wpa2_result wpa2_sm_rx(struct wpa2_sm *sm, const uint8_t *frame, size_t len);

#ifdef __cplusplus
}
#endif
