#pragma once

// An open access point: one BSS, one channel, up to four clients.
//
//     ApConfig config = {.ssid = "camera", .channel = 6};
//     if (!wifi_ap_start(&config)) return -1;
//     ...
//     wifi_ap_stop();
//
// The AP answers 192.168.4.1/24 and hands clients 192.168.4.2 to 192.168.4.5. There
// is no security, and a client that vanishes without saying goodbye keeps its slot
// until it returns.

#include <stdbool.h>
#include <stdint.h>

struct ApConfig {
    const char *ssid;    // 1 to 32 bytes
    uint8_t     channel; // 1..14
};

// Brings the interface up if needed, starts beaconing and the DHCP server. Blocks
// until the LMAC has confirmed. False when nothing was started: bad arguments,
// already running, or a step refused.
bool wifi_ap_start(const ApConfig *config);

// Removes the clients, stops beaconing and the DHCP server. False when no AP was
// running or the LMAC refused.
bool wifi_ap_stop(void);

// True from a successful wifi_ap_start() until wifi_ap_stop().
bool wifi_ap_is_up(void);
