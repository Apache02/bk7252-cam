#pragma once

// The station interface, as an application sees it: start it, wait for the
// outcome, read its state — the same shape as wifi/ap.h and wifi/scan.h.
//
//     StaConfig config = {};
//     config.ssid       = "MyNet";
//     config.passphrase = "secret123";
//
//     Shared<Promise<ConnectResult>> promise = wifi_sta_start(&config);
//     if (promise.empty()) return -1;       // refused — nothing was started
//
//     Owned<ConnectResult> result = promise->await();
//
//     if (result->status == ConnectStatus::Associated) { ... }
//     ...
//     wifi_sta_stop();
//
// Both handles free what they hold when they go out of scope, so there is
// nothing to delete, and dropping the promise without awaiting is safe — the
// association session holds a reference of its own until it resolves.
//
// Once the association is in, the link is raised and DHCP started before the
// promise resolves; there is nothing more to call. When the link is lost the
// library only clears wifi_sta_is_link_up(). It never reconnects: call
// wifi_sta_start() again.
//
// SM_CONNECT_CFM only says the request was accepted; the association itself
// lands later as SM_CONNECT_IND, and that is what resolves the promise.

#include <stdbool.h>
#include <stdint.h>

#include "platform/Promise.h"

// Associated — the link is usable. For a secured network that means the
//              handshake is done and the control port is open.
// Refused    — the AP replied with an 802.11 status code; `status_code` has it.
// NoReply    — no indication arrived before the deadline.
// AuthFailed — the association went through but the handshake did not finish:
//              a wrong passphrase, or an AP that stopped answering. The BSS has
//              been left, so another attempt can start.
//
// Spelled NoReply rather than Timeout because these enumerators are unscoped and
// wifi/scan.h already publishes a Timeout.
typedef enum : uint8_t {
    Associated,
    Refused,
    NoReply,
    AuthFailed,
} ConnectStatus;

struct ConnectResult {
    ConnectStatus status;
    uint16_t      status_code; // 802.11 status code; 0 when associated
    uint8_t       bssid[6];
};

struct StaConfig {
    const char *ssid; // required, NUL-terminated

    // NULL joins an open network. Otherwise, a WPA2-Personal passphrase of 8 to
    // 63 printable ASCII characters. It is hashed into a key before this returns
    // from wifi_sta_start(), which takes about half a second, and is not kept.
    const char *passphrase;

    // Together these let the LMAC join the BSS straight away. Leave either out
    // and this scans for the SSID first to find them — see below.
    uint8_t bssid[6]; // all zeroes = not known
    uint8_t channel;  // 1..14, or 0 = not known
};

// Brings the interface up if needed, starts an association, open or
// WPA2-Personal, and returns the promise that will carry its outcome. Everything
// config points at is copied before this returns, so it may live on the
// caller's stack.
//
// Without a BSSID and channel this first runs a scan for the SSID, and so blocks
// for as long as one sweep takes. The LMAC's own SSID cache is wiped at the start
// of every scan and empty until one has run, so it cannot serve the first
// connection after boot. Pass a BSSID and channel and no scan happens.
//
// Returns an empty handle when nothing was started: the SSID was not found, the
// link is already up, an association is already in flight, the passphrase is not
// 8 to 63 printable characters, the LMAC refused the request outright, or memory
// ran out. Then there is nothing to await. A failed association is not an empty
// handle: it resolves with Refused, NoReply or AuthFailed, and the interface
// stays up for another attempt.
Shared<Promise<ConnectResult>> wifi_sta_start(const StaConfig *config);

// Leaves the BSS, takes the link down and marks the interface down. Returns false
// when the interface was not up, or when the LMAC refused the request, which
// includes an association still being in flight — SM only acts on this once it is
// idle. The interface then stays marked up, so the call can be repeated.
//
// The deauthentication itself lands later as SM_DISCONNECT_IND, so the link is
// already down by then either way.
bool wifi_sta_stop(void);

// Brought up by wifi_sta_start() or by a scan, and not stopped since.
bool wifi_sta_is_up(void);

// Associated, and for a secured network the handshake is done. False again on a
// disconnect indication or wifi_sta_stop().
bool wifi_sta_is_link_up(void);

// The current IPv4 address, netmask and gateway in host byte order; any of them
// may be NULL. Returns false while the interface has no address yet — which is
// also how "DHCP has not finished" looks.
bool wifi_sta_get_address(uint32_t *addr, uint32_t *netmask, uint32_t *gateway);
