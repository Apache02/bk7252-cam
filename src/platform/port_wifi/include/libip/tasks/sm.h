#pragma once

// SM, task 4 — the station state machine. SM_CONNECT_REQ starts a chain of
// states and most of the ids below are steps or reports along it, not
// independent operations. docs/wifi_rw.md walks through the chain.
//
// Ids come from the archive's sm_task.h, cross-checked against sm_task.o.
// Payload layouts were read out of the archive's DWARF debug info.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "libip/ke.h"
#include "libip/mac.h"
#include "libip/tasks/scan.h"

#ifdef __cplusplus
extern "C" {
#endif

enum sm_msg_tag {
    SM_RESET_REQ = KE_FIRST_MSG(TASK_SM),
    SM_RESET_CFM,
    SM_CONNECT_REQ,
    SM_CONNECT_CFM, // only says the request was accepted, not how it turned out
    SM_CONNECT_IND, // the actual outcome
    SM_DISCONNECT_REQ,
    SM_DISCONNECT_CFM,
    SM_DISCONNECT_IND,
    SM_POWER_MGMT_REQ,
    SM_POWER_MGMT_CFM,
    SM_SYNCLOST_IND,
    SM_RSP_TIMEOUT_IND,
    SM_ROAMING_TIMER_IND,
    SM_GET_BSS_INFO_REQ,
    SM_GET_BSS_INFO_CFM,
    SM_CONNECTION_START_IND,
    SM_BEACON_LOSE_IND,
    SM_AUTHEN_FAIL_IND,
    SM_ASSOC_FAIL_INID, // vendor typo, kept verbatim — not SM_ASSOC_FAILED_IND
    SM_ASSOC_IND,
    SM_DEASSOC_IND,
    SM_ASSOC_FAILED_IND,
};

// What ke_state_get(TASK_SM) returns. The chain SM_CONNECT_REQ walks through,
// in order, ending back at SM_IDLE whether or not it associated.
enum sm_state_tag {
    SM_IDLE = 0,
    SM_SCANNING,
    SM_JOINING,
    SM_STA_ADDING,
    SM_DISABLING_PS,
    SM_BSS_PARAM_SETTING,
    SM_AUTHENTICATING,
    SM_ASSOCIATING,
    SM_ACTIVATING,
    SM_DISCONNECTING,
    SM_STATE_MAX,
};

struct sm_connect_req {
    struct mac_ssid      ssid;
    struct mac_addr      bssid;
    struct scan_chan_tag chan;
    uint32_t             flags;
    uint16_t             ctrl_port_ethertype;
    uint16_t             ie_len;
    uint16_t             listen_interval;
    bool                 dont_wait_bcmc;
    uint8_t              auth_type;
    uint8_t              uapsd_queues;
    uint8_t              vif_idx;
    uint32_t             ie_buf[64];
    uint16_t             bcn_len;
    int8_t               rssi;
    uint16_t             cap_info;
    uint16_t             beacon_period;
    // uint32_t bcn_buf[] follows; allocate sizeof(*req) + bcn_len to use it.
};

struct sm_connect_cfm {
    uint8_t status;
};
static_assert(sizeof(struct sm_connect_cfm) == 1, "sm_task.o DWARF: sm_connect_cfm is 1 byte");

// Only the head is described here; the 836-byte message continues with the
// association request and response IEs, which nothing in this port reads.
struct sm_connect_indication {
    uint16_t status_code; // 802.11 status code, 0 on success
    uint8_t  bssid[6];
    uint8_t  roamed;
    uint8_t  vif_idx;
    uint8_t  ap_idx;
    uint8_t  ch_idx;
    uint8_t  qos;
    uint8_t  acm;
};
static_assert(offsetof(struct sm_connect_indication, bssid) == 2, "sm_task.o DWARF");
static_assert(offsetof(struct sm_connect_indication, ch_idx) == 11, "sm_task.o DWARF");
static_assert(sizeof(struct sm_connect_indication) == 14, "sm_task.o DWARF");

// SM_DISCONNECT_CFM carries no parameters: the handler answers with
// ke_msg_send_basic(), and only after SM has reached SM_IDLE — a request that
// arrives mid-association is saved and replayed on the next state change.
struct sm_disconnect_req {
    uint16_t reason_code; // an 802.11 reason, mac_frame.h's MAC_RS_*
    uint8_t  vif_idx;
};
static_assert(sizeof(struct sm_disconnect_req) == 4, "sm_task.o DWARF: sm_disconnect_req is 4 bytes");
static_assert(offsetof(struct sm_disconnect_req, vif_idx) == 2, "sm_task.o DWARF");

// "Deauthenticated because sending station is leaving", which is what a host
// asking to leave means (MAC_RS_DEAUTH_SENDER_LEFT_IBSS_ESS in mac_frame.h).
#define MAC_RS_SENDER_LEAVING 3

struct sm_disconnect_ind {
    uint16_t reason_code;
    uint8_t  vif_idx;
    uint8_t  ft_over_ds;
};
static_assert(sizeof(struct sm_disconnect_ind) == 4, "sm_task.o DWARF: sm_disconnect_ind is 4 bytes");

#ifdef __cplusplus
}
#endif
