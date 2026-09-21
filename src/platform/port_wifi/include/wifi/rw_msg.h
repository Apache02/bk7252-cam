#pragma once

// Host to LMAC requests (rw_msg.c). Each call builds a ke_msg and hands it to
// the Wi-Fi core task. Those with a confirmation block until it arrives or the
// send times out; all of them return 0 on success.
//
// The bring-up order the LMAC expects is reset, me_config, me_chan_config,
// start, then add_if — nothing later works if one of them is skipped.

#include <stdbool.h>
#include <stdint.h>

// For struct mm_add_if_cfm, which rw_msg_send_add_if() fills in.
#include "libip/tasks/mm.h"
// For VIF_STA and the rest of enum vif_type, which rw_msg_send_add_if() takes.
#include "libip/vif.h"

#ifdef __cplusplus
extern "C" {
#endif

int rw_msg_send_reset(void);
int rw_msg_send_me_config_req(void);
int rw_msg_send_me_chan_config_req(void);
int rw_msg_send_start(void);
int rw_msg_send_add_if(uint8_t type, const uint8_t mac[6], struct mm_add_if_cfm *cfm);

// Sends a request nobody is waiting on. The caller builds the ke_msg itself with
// ke_msg_alloc() and hands it over here; 0 means it reached the WiFi core task's
// queue, and non-zero means it did not and has already been freed.
//
// Scanning uses this — see wifi/scan.h — because its result comes back as
// indications rather than as a confirmation.
int rw_msg_post(void *msg_params);

// Channel 1-14. Returns only once the LMAC has actually retuned. band_width and
// cfm exist because libip(rwnx.o) imports this function and calls it with three
// arguments; this port ignores cfm.
int rw_msg_set_channel(uint32_t channel, uint32_t band_width, void *cfm);

// Asks for an open (no WPA) association and waits only for SM_CONNECT_CFM, which
// says the request was accepted — non-zero here means it was not, and no
// indication will follow. The association itself lands later as SM_CONNECT_IND;
// wifi/connect.h is the API that waits for that, and the one applications want.
//
// bssid is six bytes and channel is 1..14; together they make the LMAC join that
// BSS directly. FF:FF:FF:FF:FF:FF instead sends it to its own scan cache, and to
// a fresh scan when that misses.
int rw_msg_send_sm_connect_req(uint8_t vif_idx, const char *ssid, const uint8_t bssid[6], uint8_t channel);

// Leaves the BSS, with an 802.11 reason code (libip/tasks/sm.h publishes
// MAC_RS_SENDER_LEAVING for the ordinary one). The confirmation says only that
// SM acted; SM_DISCONNECT_IND reports the link actually going.
//
// SM handles this only once it is idle, so a request sent mid-association is
// held by the LMAC and replayed, and this call then times out.
int rw_msg_send_sm_disconnect_req(uint8_t vif_idx, uint16_t reason_code);

#ifdef __cplusplus
}
#endif
