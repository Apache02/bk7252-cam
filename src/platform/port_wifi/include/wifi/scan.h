#pragma once


// Scanning, as an application sees it: start one, wait for it, walk what came
// back.
//
//     Shared<Promise<ScanResult>> promise = wifi_scan(nullptr);
//     if (promise.empty()) return -1;       // refused — nothing was started
//
//     Owned<ScanResult> result = promise->await();
//
//     for (const ScanStation *s = result->stations; s; s = s->next) { ... }
//
// Awaiting is what transfers the result out of the promise. Both handles free
// what they hold when they go out of scope, so there is nothing to delete, and
// dropping the promise without awaiting is safe — the scan session holds a
// reference of its own until it resolves.

#include <stdint.h>

#include "platform/Promise.h"

// Ok        — the LMAC reported the sweep finished.
// Partial   — it finished, but at least one beacon was lost to a failed
//             allocation; `dropped` says how many.
// Timeout   — the sweep ran past its budget and was canceled. The stations
//             collected before that are still here, but the remaining channels
//             were never visited.
typedef enum : uint8_t {
    Ok,
    Partial,
    Timeout,
} ScanStatus;

// Weakest first — a network offering both WPA and WPA2 reports WPA2.
typedef enum : uint8_t {
    Open,
    Wep,
    Wpa,
    Wpa2,
    Wpa3,
} StationSecurity;

#define SCAN_SSID_MAX 32

struct ScanStation {
    ScanStation    *next;
    uint8_t         bssid[6];
    uint8_t         channel;
    int8_t          rssi;
    StationSecurity security;
    uint8_t         ssid_len; // 0 for a hidden network
    uint16_t        beacon_int;
    char            ssid[SCAN_SSID_MAX];
};

// One per channel per sweep. Busy time is the useful number: a channel with real
// traffic on it that yielded no beacons points at the receiver, not at an empty
// band.
struct ChannelSurvey {
    ChannelSurvey *next;
    uint16_t       freq;
    int8_t         noise_dbm;
    uint32_t       time_ms;
    uint32_t       busy_ms;
};

struct ScanResult {
    ScanStatus     status;
    ScanStation   *stations;
    ChannelSurvey *survey;
    uint16_t       station_count;
    uint16_t       indications; // every indication, duplicates included
    uint16_t       dropped;     // beacons lost to a failed allocation

    ~ScanResult();
};

// How long the receiver stays parked on one channel — the LMAC offers only these
// two. Beacons come about every 100 ms, so the longer setting finds networks the
// short one can miss, at the cost of a slower sweep. It does not make the scan
// passive; see SCAN_PASSIVE_BIT in libip/tasks/scan.h for why.
#define SCAN_CHANNEL_TIME_SHORT_MS 90
#define SCAN_CHANNEL_TIME_LONG_MS  110

// Every field zeroed is the default: every channel, no filtering, 90 ms.
struct ScanConfig {
    // Accept only this BSSID; all zeroes accepts every network.
    //
    // Not a mask. The LMAC compares all six bytes, unless the group bit (bit 0 of
    // the first byte) is set, which turns the filter off entirely — so
    // FF:FF:FF:00:00:00 matches everything rather than an OUI prefix.
    uint8_t bssid[6];

    // Directed probe requests, NUL-terminated; null or empty leaves the probe
    // undirected. The LMAC carries at most these two.
    const char *ssid;
    const char *ssid_alt;

    // 1..14 to sweep a single channel, 0 for all of them. One channel also
    // shortens the deadline the sweep is given.
    uint8_t channel;

    bool longer_channel_time;

    // Keep the per-channel survey. Off by default because it costs an allocation
    // per channel in the task that also delivers the LMAC's replies.
    bool collect_survey;
};

// Starts a sweep and returns the promise that will carry its result; nullptr for
// config means the defaults above.
//
// A sweep runs from the station interface, and this brings it up if it is not up
// yet, so there is no separate start step to call first.
//
// Everything config points at is copied before this returns, strings included,
// so it may live on the caller's stack.
//
// Returns an empty handle when nothing was started — the station interface
// could not be brought up, a scan is already in flight, the request could not
// be queued, or memory ran out. Then there is nothing to await.
Shared<Promise<ScanResult>> wifi_scan(const ScanConfig *config);
