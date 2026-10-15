#pragma once

// Host to LMAC requests (rw_msg.c). Each call builds a ke_msg and hands it to
// the Wi-Fi core task. Those with a confirmation block until it arrives or the
// send times out; all of them return 0 on success.
//
// The bring-up order the LMAC expects is reset, me_config, me_chan_config,
// start, then add_if — nothing later works if one of them is skipped.

#include <stdbool.h>
#include <stdint.h>

// For struct apm_start_cfm, which rw_msg_send_apm_start_req() fills in.
#include "libip/tasks/apm.h"
// For struct me_sta_add_cfm, which rw_msg_send_me_sta_add() fills in.
#include "libip/tasks/me.h"
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

// Asks for an association and waits only for SM_CONNECT_CFM, which says the
// request was accepted — non-zero here means it was not, and no indication will
// follow. The association itself lands later as SM_CONNECT_IND;
// wifi/sta.h is the API that waits for that, and the one applications want.
//
// bssid is six bytes and channel is 1..14; together they make the LMAC join that
// BSS directly. FF:FF:FF:FF:FF:FF instead sends it to its own scan cache, and to
// a fresh scan when that misses.
//
// ie is NULL for an open network. For WPA2 it is the RSN IE (at most 256 bytes),
// and the request is flagged as a host-controlled port with WPA in use.
int rw_msg_send_sm_connect_req(uint8_t vif_idx, const char *ssid, const uint8_t bssid[6], uint8_t channel,
                               const uint8_t *ie, uint16_t ie_len);

// Leaves the BSS, with an 802.11 reason code (libip/tasks/sm.h publishes
// MAC_RS_SENDER_LEAVING for the ordinary one). The confirmation says only that
// SM acted; SM_DISCONNECT_IND reports the link actually going.
//
// SM handles this only once it is idle, so a request sent mid-association is
// held by the LMAC and replayed, and this call then times out.
int rw_msg_send_sm_disconnect_req(uint8_t vif_idx, uint16_t reason_code);

// ---- Access point ---------------------------------------------------------

// Starts beaconing on a VIF added with VIF_AP. beacon is the whole template, from
// the MAC header; tim_oft and tim_len say where its TIM element is. The LMAC
// downloads the template during the call, but keep it for as long as the AP runs.
int rw_msg_send_apm_start_req(uint8_t vif_idx, uint8_t channel, const uint8_t *beacon, uint16_t beacon_len,
                              uint16_t tim_oft, uint8_t tim_len, struct apm_start_cfm *cfm);
int rw_msg_send_apm_stop_req(uint8_t vif_idx);

// Registers an associated client with the LMAC, and opens or closes its control
// port. APM_ASSOC_IND is sent once the port is opened.
int rw_msg_send_me_sta_add(uint8_t vif_idx, const uint8_t mac[6], uint16_t aid, struct me_sta_add_cfm *cfm);
int rw_msg_send_me_sta_del(uint8_t sta_idx);
int rw_msg_send_me_set_control_port(uint8_t sta_idx, bool open);

// Installs a 16-byte CCMP key. sta_idx is the AP's index (at most 3) for a
// pairwise key, with key_idx 0, or 0xFF for a group key, with key_idx 0 to 3.
// Anything else is refused here because the LMAC hangs on it. The hardware key
// slot it chose comes back in hw_key_idx.
int rw_msg_send_mm_key_add(uint8_t vif_idx, uint8_t sta_idx, uint8_t key_idx, const uint8_t key[16],
                           uint8_t *hw_key_idx);

// Frees a key slot that rw_msg_send_mm_key_add() returned. A key left behind
// stays in use by the next association with the same AP index, even an open one.
int rw_msg_send_mm_key_del(uint8_t hw_key_idx);

// Transmits one management frame, MAC header first. The frame is copied before
// the call returns, but the LMAC edits its first byte in place, so it must be
// writable.
int rw_msg_send_mgmt_tx(uint8_t vif_idx, uint8_t *frame, uint16_t len);

#ifdef __cplusplus
}
#endif
