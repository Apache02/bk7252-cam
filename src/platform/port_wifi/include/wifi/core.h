#pragma once

// WiFi bring-up entry points, in the order an application calls them: the radio
// has to be powered (sctrl_rf_init()) before rwnxl_init(), and the LMAC has to
// exist before wifi_core_start() gives it tasks to run on. Then the few things
// that are true for the whole port rather than for scanning or associating:
// the station address, the MAC reset count, the channel table.

#include <stdbool.h>
#include <stdint.h>

// For rwnxl_init(), the vendor call that opens the sequence below.
#include "libip/rwnx.h"

#ifdef __cplusplus
extern "C" {
#endif

// wifi_core.c — creates the core and kmsg tasks and the message queue between
// them. Nothing may send an LMAC request before this returns.
void wifi_core_start(void);

// wifi_core.c — has wifi_core_start() run? Reading LMAC state or MAC registers
// before it has is meaningless, and the MAC block is not clocked yet.
bool wifi_core_running(void);

// mac_glue.c — how many times the vendor binaries have restarted the MAC since
// boot. Each restart resets the radio, the transmit and receive paths and the
// MM state, so a link rarely survives one.
uint32_t wifi_mac_resets(void);

// mac_glue.c — the 2.4 GHz channel table, the port's only copy. Whoever builds
// a channel list or reads one back asks here rather than carrying a second one.
// The archive imports the first of the two.

// MHz to channel id 1-14; 0 if the frequency is not a channel center.
uint8_t rw_ieee80211_get_chan_id(uint16_t freq);

// Channel id 1-14 to center frequency in MHz; 0 if the id is out of range.
uint16_t rw_ieee80211_get_centre_frequency(uint32_t chan_id);

#ifdef __cplusplus
}
#endif
