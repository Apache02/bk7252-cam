#pragma once

// ME, task 3 — per-station config, management-frame TX and rate control.
//
// Ids come from the archive's me_task.h, cross-checked against me_task.o.
// RC_ENABLE was on in this build (me_rc_stats_req_handler and
// me_rc_set_rate_req_handler are both present), so the three RC_* ids are real
// rather than compiled out.
//
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

enum me_msg_tag {
    ME_CONFIG_REQ = KE_FIRST_MSG(TASK_ME),
    ME_CONFIG_CFM,
    ME_CHAN_CONFIG_REQ,
    ME_CHAN_CONFIG_CFM,
    ME_SET_CONTROL_PORT_REQ,
    ME_SET_CONTROL_PORT_CFM,
    ME_TKIP_MIC_FAILURE_IND,
    ME_MGMT_TX_REQ,
    ME_MGMT_TX_CFM,
    ME_MGMT_TX_DONE_IND,
    ME_STA_ADD_REQ,
    ME_STA_ADD_CFM,
    ME_STA_DEL_REQ,
    ME_STA_DEL_CFM,
    ME_TX_CREDITS_UPDATE_IND,
    ME_UAPSD_TRAFFIC_IND_REQ,
    ME_UAPSD_TRAFFIC_IND_CFM,
    ME_RC_STATS_REQ,
    ME_RC_STATS_CFM,
    ME_RC_SET_RATE_REQ,

    // internal from here down — these never reach the host
    ME_SET_ACTIVE_REQ,
    ME_SET_ACTIVE_CFM,
    ME_SET_PS_DISABLE_REQ,
    ME_SET_PS_DISABLE_CFM,
    ME_PS_REQ,
};

struct me_config_req {
    struct mac_htcapability  ht_cap;
    struct mac_vhtcapability vht_cap;
    uint16_t                 tx_lft;
    bool                     ht_supp;
    bool                     vht_supp;
    bool                     ps_on;
};

struct me_chan_config_req {
    struct scan_chan_tag chan2G4[SCAN_CHANNEL_2G4];
    struct scan_chan_tag chan5G[SCAN_CHANNEL_5G];
    uint8_t              chan2G4_cnt;
    uint8_t              chan5G_cnt;
};

struct me_set_control_port_req {
    uint8_t sta_idx;
    bool    control_port_open;
};

struct me_mgmt_tx_req {
    uint32_t addr;   // 0, host address of the frame
    uint32_t hostid; // 4, the same address; handed back in the confirmation
    uint16_t len;    // 8
    bool     no_cck;
    bool     no_ack;
    bool     off_channel;
    uint8_t  vif_idx;         // 13
    uint32_t req_malloc_flag; // 16
};

struct me_mgmt_tx_cfm {
    uint32_t hostid; // 0, the LMAC's copy of the frame on an AP interface, already freed
    uint8_t  status;
    bool     rsp_frm_new;
    uint16_t length;
};

struct me_sta_add_req {
    struct mac_addr          mac_addr;     // 0
    struct mac_rateset       rate_set;     // 6
    struct mac_htcapability  ht_cap;       // 20
    struct mac_vhtcapability vht_cap;      // 52
    uint32_t                 flags;        // 64
    uint16_t                 aid;          // 68
    uint8_t                  uapsd_queues; // 70
    uint8_t                  opmode;       // 71
    uint8_t                  vif_idx;      // 72
    bool                     tdls_sta;     // 73
};

struct me_sta_add_cfm {
    uint8_t sta_idx;
    uint8_t status;
    uint8_t pm_state;
};

struct me_sta_del_req {
    uint8_t sta_idx;
    bool    tdls_sta;
};

static_assert(sizeof(struct me_set_control_port_req) == 2, "me_task.o DWARF");
static_assert(sizeof(struct me_mgmt_tx_req) == 20, "me_task.o DWARF");
static_assert(offsetof(struct me_mgmt_tx_req, len) == 8, "");
static_assert(offsetof(struct me_mgmt_tx_req, vif_idx) == 13, "");
static_assert(offsetof(struct me_mgmt_tx_req, req_malloc_flag) == 16, "");
static_assert(sizeof(struct me_mgmt_tx_cfm) == 8, "me_task.o DWARF");
static_assert(sizeof(struct me_sta_add_req) == 76, "me_task.o DWARF");
static_assert(offsetof(struct me_sta_add_req, rate_set) == 6, "");
static_assert(offsetof(struct me_sta_add_req, ht_cap) == 20, "");
static_assert(offsetof(struct me_sta_add_req, vht_cap) == 52, "");
static_assert(offsetof(struct me_sta_add_req, flags) == 64, "");
static_assert(offsetof(struct me_sta_add_req, aid) == 68, "");
static_assert(offsetof(struct me_sta_add_req, vif_idx) == 72, "");
static_assert(sizeof(struct me_sta_add_cfm) == 3, "me_task.o DWARF");
static_assert(sizeof(struct me_sta_del_req) == 2, "me_task.o DWARF");

#ifdef __cplusplus
}
#endif
