#pragma once

// What one LMAC indication does to a connect session: the seam that lets
// rwnx_intf.c, which is C, drive a session written in C++. Both run in the
// kmsg task. Only the message dispatcher calls these; wifi/sta.h is what an
// application uses.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// SM_CONNECT_IND — the association either completed or was refused. Resolves the
// promise, and tells the archive we are connected so its power-save and
// traffic-detection code stops treating the station as idle.
void sm_connect_ind_handler(const void *param, uint16_t param_len);

// SM_DISCONNECT_IND — the link is gone. Only updates the station status; nothing
// is waiting on it.
void sm_disconnect_ind_handler(const void *param, uint16_t param_len);

// The supplicant's verdict on a WPA2 handshake. Resolves the connect session:
// Associated when true, AuthFailed when false.
void wpa2_result_handler(bool ok);

// Drops the link and asks the LMAC to leave the BSS. wifi_sta_stop() is this plus
// marking the interface down; a failed handshake calls it alone, so the interface
// stays up for another attempt. False when the LMAC refused.
bool connect_leave(uint8_t vif_idx);

#ifdef __cplusplus
}
#endif
