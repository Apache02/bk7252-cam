#pragma once

// What the C parts of the port hand to the AP, which is written in C++: the
// LMAC's management-frame hook, its two indications, and the data path's lookup of
// a client. See wifi/ap.h for the API an application uses.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// No such client, where a station index is expected.
#define AP_NO_STATION 0xFF

// A management frame the LMAC received on an AP interface, MAC header first. Runs
// in the core task: copies the frame and returns at once.
void ap_mgmt_rx(const uint8_t *frame, uint16_t len, uint8_t vif_idx);

// APM_ASSOC_IND and APM_DEASSOC_IND, from the kmsg task. Both carry the client's
// MAC address in their first six bytes.
void ap_assoc_ind_handler(const void *param, uint16_t param_len);
void ap_deassoc_ind_handler(const void *param, uint16_t param_len);

// The LMAC station index to send a frame for this destination by: the client's own,
// the broadcast station for group addresses, AP_NO_STATION for an unknown client.
uint8_t ap_tx_staid(const uint8_t dest[6]);

#ifdef __cplusplus
}
#endif
