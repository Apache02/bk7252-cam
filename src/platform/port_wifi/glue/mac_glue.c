#include <stdint.h>
#include <stddef.h>

#include "libip/rwnx.h"
#include "wifi/core.h"
#include "wifi/net.h"
#include "wifi/rw_msg.h"

#define DEBUG_NAME "mac_glue"
#include "debug.h"

// 2.4 GHz center frequencies indexed by channel number (index = channel - 1).
static const uint16_t s_2ghz_freq[14] = {
    2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484,
};

// ---- IEEE 802.11 channel API ------------------------------------------------

// imported: libip(rwnx.o)
// The vendor builds a country/regulatory context here. This port leaves the
// regulatory domain implicit and scans every 2.4 GHz channel.
uint32_t rw_ieee80211_init(void) { return 0; }

// imported: libip(apm.o, mm_bcn.o) — MHz to channel id (1-14), 0 = not found
uint8_t rw_ieee80211_get_chan_id(uint16_t freq) {
    for (uint8_t i = 0; i < 14; i++) {
        if (s_2ghz_freq[i] == freq) return (uint8_t)(i + 1u);
    }
    return 0;
}

// channel id (1-14) to center frequency in MHz, 0 = out of range
uint16_t rw_ieee80211_get_centre_frequency(uint32_t chan_id) {
    if (chan_id < 1 || chan_id > 14) return 0;
    return s_2ghz_freq[chan_id - 1];
}

// ---- MAC management ---------------------------------------------------------

// imported: libip(sm.o)
// VIF_INF_PTR is an opaque pointer into the LMAC's own vif_info_tab. NULL means
// "no interface of that type", and sm.o guards on it before dereferencing.
void *rwm_mgmt_vif_type2ptr(uint8_t vif_type) {
    (void)vif_type;
    return NULL;
}

// imported: libip(rwnx.o) — init the TX MSDU pool; nothing to do without AP PS
void rwm_msdu_init(void) {}

// ---- Hardware patch hooks ---------------------------------------------------
// SDK (rw_platf_pub.c): on BK7221U with CFG_USE_MCU_PS=0 these are no-ops. Only
// the two the binaries import are kept.

// imported: libip(rwnx.o)
// rwnxl_reset_handle() calls this on its way through a MAC reset, which is the
// only warning the host gets that the transmit descriptor ring is about to be
// zeroed.
static uint32_t s_mac_resets;

void rwxl_reset_patch(void) {
    s_mac_resets++;
    wifi_net_tx_reset();
    rwnxl_violence_reset_patch();
}

uint32_t wifi_mac_resets(void) { return s_mac_resets; }

// imported: libip(hal_machw.o)
// The vendor points the MAC's debug port mux at the PHY here. Left empty: it is
// a diagnostics aid with no effect on operation.
void hal_machw_init_diagnostic_ports(void) {}
