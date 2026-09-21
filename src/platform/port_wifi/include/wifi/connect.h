#pragma once


// Associating, as an application sees it — the same shape as wifi/scan.h: start
// one, wait for it, read it.
//
//     ConnectConfig config = {};
//     config.ssid = "MyNet";
//
//     Shared<Promise<ConnectResult>> promise = wifi_connect_start(vif_idx, &config);
//     if (promise.empty()) return -1;       // refused — nothing was started
//
//     Owned<ConnectResult> result = promise->await();
//
//     if (result->status == ConnectStatus::Associated) { ... }
//
// Both handles free what they hold when they go out of scope, so there is
// nothing to delete, and dropping the promise without awaiting is safe — the
// association session holds a reference of its own until it resolves.
//
// SM_CONNECT_CFM only says the request was accepted; the association itself
// lands later as SM_CONNECT_IND, and that is what resolves the promise.

#include <stdbool.h>
#include <stdint.h>

#include "platform/Promise.h"

// Associated — the LMAC reported success.
// Refused    — the AP replied with an 802.11 status code; `status_code` has it.
// NoReply    — no indication arrived before the deadline.
//
// Spelled NoReply rather than Timeout because these enumerators are unscoped and
// wifi/scan.h already publishes a Timeout.
typedef enum : uint8_t {
    Associated,
    Refused,
    NoReply,
} ConnectStatus;

struct ConnectResult {
    ConnectStatus status;
    uint16_t      status_code; // 802.11 status code; 0 when associated
    uint8_t       bssid[6];
    uint8_t       vif_idx;
    uint8_t       ap_idx;  // the AP's index in the LMAC's station table
    uint8_t       ch_idx;
};

struct ConnectConfig {
    const char *ssid; // required, NUL-terminated

    // Together these let the LMAC join the BSS straight away. Leave either out
    // and this scans for the SSID first to find them — see below.
    uint8_t bssid[6]; // all zeroes = not known
    uint8_t channel;  // 1..14, or 0 = not known
};

// Starts an open (no WPA) association and returns the promise that will carry
// its outcome. Everything config points at is copied before this returns, so it
// may live on the caller's stack.
//
// Without a BSSID and channel this first runs a scan for the SSID, and so blocks
// for as long as one sweep takes. The LMAC's own SSID cache is wiped at the start
// of every scan and empty until one has run, so it cannot serve the first
// connection after boot. Pass a BSSID and channel and no scan happens.
//
// Returns an empty handle when nothing was started: the SSID was not found, an
// association is already in flight, the LMAC refused the request outright, or
// memory ran out. Then there is nothing to await.
Shared<Promise<ConnectResult>> wifi_connect_start(uint8_t vif_idx, const ConnectConfig *config);

// Leaves the BSS and takes the interface's link down. Returns false when the
// LMAC refused the request, which includes an association still being in
// flight — SM only acts on this once it is idle.
//
// Returns true once the request was accepted. The deauthentication itself lands
// later as SM_DISCONNECT_IND, so the link is already down by then either way.
bool wifi_disconnect(uint8_t vif_idx);
