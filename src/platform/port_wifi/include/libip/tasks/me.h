#pragma once

// ME, task 3 — per-station config, management-frame TX and rate control.
//
// Ids come from the archive's me_task.h, cross-checked against me_task.o.
// RC_ENABLE was on in this build (me_rc_stats_req_handler and
// me_rc_set_rate_req_handler are both present), so the three RC_* ids are real
// rather than compiled out.
//
// Payload layouts were read out of the archive's DWARF debug info.

#include <stdbool.h>
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

#ifdef __cplusplus
}
#endif
