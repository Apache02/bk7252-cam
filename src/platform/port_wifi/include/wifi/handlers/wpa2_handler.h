#pragma once

// The supplicant's entry points, for the C parts of the port (the data path and
// the connect session's result). Applications use wifi/connect.h instead.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Arms the supplicant for one handshake with the AP at bssid, before the
// association is requested, so an EAPOL frame that beats SM_CONNECT_IND is not
// lost. Starts the task on the first call. False when it could not start.
bool wpa2_sta_arm(uint8_t vif_idx, const uint8_t pmk[32], const uint8_t bssid[6]);

// Forgets the handshake. Nothing is sent to the AP.
void wpa2_sta_disarm(void);

// Gives the handshake up: leaves the BSS and reports failure to the connect
// session. Called by the session's timer, which must not block.
void wpa2_sta_abort(void);

// An Ethernet frame of ethertype 0x888E that the radio received. Runs on the
// Wi-Fi core task: copies the frame and returns at once.
void wpa2_eapol_rx(uint8_t vif_idx, const uint8_t *frame, uint16_t len);

#ifdef __cplusplus
}
#endif
