#pragma once

// What one LMAC indication does to a connect session: the seam that lets
// rwnx_intf.c, which is C, drive a session written in C++. Both run in the
// kmsg task. Only the message dispatcher calls these; wifi/connect.h is what an
// application uses.

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

#ifdef __cplusplus
}
#endif
