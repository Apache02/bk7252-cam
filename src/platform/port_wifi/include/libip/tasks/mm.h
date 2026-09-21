#pragma once

// MM, task 0 — hardware, channels and virtual interfaces. By far the biggest
// task in the archive, and the one every bring-up step goes through.
//
// Ids come from the archive's own mm_task.h, cross-checked against mm_task.o.
// Payload layouts were read out of the archive's DWARF debug info.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "libip/ke.h"
#include "libip/mac.h"

#ifdef __cplusplus
extern "C" {
#endif

enum mm_msg_tag {
    MM_RESET_REQ = KE_FIRST_MSG(TASK_MM),
    MM_RESET_CFM,
    MM_START_REQ,
    MM_START_CFM,
    MM_VERSION_REQ,
    MM_VERSION_CFM,
    MM_ADD_IF_REQ,
    MM_ADD_IF_CFM,
    MM_REMOVE_IF_REQ,
    MM_REMOVE_IF_CFM,
    MM_STA_ADD_REQ,
    MM_STA_ADD_CFM,
    MM_STA_DEL_REQ,
    MM_STA_DEL_CFM,
    MM_SET_FILTER_REQ,
    MM_SET_FILTER_CFM,
    MM_SET_CHANNEL_REQ,
    MM_SET_CHANNEL_CFM,
    MM_SET_DTIM_REQ,
    MM_SET_DTIM_CFM,
    MM_SET_BEACON_INT_REQ,
    MM_SET_BEACON_INT_CFM,
    MM_SET_BASIC_RATES_REQ,
    MM_SET_BASIC_RATES_CFM,
    MM_SET_BSSID_REQ,
    MM_SET_BSSID_CFM,
    MM_SET_EDCA_REQ,
    MM_SET_EDCA_CFM,
    MM_SET_MODE_REQ,
    MM_SET_MODE_CFM,
    MM_SET_VIF_STATE_REQ,
    MM_SET_VIF_STATE_CFM,
    MM_SET_SLOTTIME_REQ,
    MM_SET_SLOTTIME_CFM,
    MM_SET_IDLE_REQ,
    MM_SET_IDLE_CFM,
    MM_KEY_ADD_REQ,
    MM_KEY_ADD_CFM,
    MM_KEY_DEL_REQ,
    MM_KEY_DEL_CFM,
    MM_BA_ADD_REQ,
    MM_BA_ADD_CFM,
    MM_BA_DEL_REQ,
    MM_BA_DEL_CFM,
    MM_PRIMARY_TBTT_IND,
    MM_SECONDARY_TBTT_IND,
    MM_SET_POWER_REQ,
    MM_SET_POWER_CFM,
    MM_DBG_TRIGGER_REQ, // no confirmation of its own
    MM_SET_PS_MODE_REQ,
    MM_SET_PS_MODE_CFM,
    MM_CHAN_CTXT_ADD_REQ,
    MM_CHAN_CTXT_ADD_CFM,
    MM_CHAN_CTXT_DEL_REQ,
    MM_CHAN_CTXT_DEL_CFM,
    MM_CHAN_CTXT_LINK_REQ,
    MM_CHAN_CTXT_LINK_CFM,
    MM_CHAN_CTXT_UNLINK_REQ,
    MM_CHAN_CTXT_UNLINK_CFM,
    MM_CHAN_CTXT_UPDATE_REQ,
    MM_CHAN_CTXT_UPDATE_CFM,
    MM_CHAN_CTXT_SCHED_REQ,
    MM_CHAN_CTXT_SCHED_CFM,
    MM_BCN_CHANGE_REQ,
    MM_BCN_CHANGE_CFM,
    MM_TIM_UPDATE_REQ,
    MM_TIM_UPDATE_CFM,
    MM_CONNECTION_LOSS_IND,
    // Sent around every channel change. A scan run while associated produces a
    // pair per channel, because the LMAC keeps stepping back to the operating
    // one.
    MM_CHANNEL_SWITCH_IND,
    MM_CHANNEL_PRE_SWITCH_IND,
    MM_REMAIN_ON_CHANNEL_REQ,
    MM_REMAIN_ON_CHANNEL_CFM,
    MM_REMAIN_ON_CHANNEL_EXP_IND,
    MM_PS_CHANGE_IND,
    MM_TRAFFIC_REQ_IND,
    MM_SET_PS_OPTIONS_REQ,
    MM_SET_PS_OPTIONS_CFM,
    MM_P2P_VIF_PS_CHANGE_IND,
    MM_CSA_COUNTER_IND,
    MM_CHANNEL_SURVEY_IND,
    MM_BFMER_ENABLE_REQ,
    MM_SET_P2P_NOA_REQ,
    MM_SET_P2P_OPPPS_REQ,
    MM_SET_P2P_NOA_CFM,
    MM_SET_P2P_OPPPS_CFM,
    MM_P2P_NOA_UPD_IND,
    MM_CFG_RSSI_REQ,
    MM_RSSI_STATUS_IND,
    MM_CSA_FINISH_IND,
    MM_CSA_TRAFFIC_IND,
    MM_MU_GROUP_UPDATE_REQ,
    MM_MU_GROUP_UPDATE_CFM,

    // internal from here down — these never reach the host
    MM_FORCE_IDLE_REQ,
    MM_SCAN_CHANNEL_START_IND,
    MM_SCAN_CHANNEL_END_IND,
    MM_GET_CHANNEL_REQ,
    MM_GET_CHANNEL_CFM,
};

// What ke_state_get(TASK_MM) returns. MM_HOST_BYPASSED is the one to watch:
// mm_hw_config_handler short-circuits MM_SET_CHANNEL_REQ there, confirming a
// retune that never happened.
enum mm_state_tag {
    MM_IDLE = 0,
    MM_ACTIVE,
    MM_GOING_TO_IDLE,
    MM_HOST_BYPASSED,
    MM_STATE_MAX,
};

// phy_cfg_tag has parameters[16] (phy.h: PHY_CFG_BUF_SIZE = 16).
struct mm_start_req {
    uint32_t phy_cfg_parameters[16];
    uint32_t uapsd_timeout;
    uint16_t lp_clk_accuracy;
};

struct mm_add_if_req {
    uint8_t         type;
    struct mac_addr addr;
    bool            p2p;
};

struct mm_add_if_cfm {
    uint8_t status;   // 0 on success
    uint8_t inst_nbr; // LMAC-assigned VIF index — the vif_idx everything else takes
};
static_assert(sizeof(struct mm_add_if_cfm) == 2, "mm_task.o DWARF: mm_add_if_cfm is 2 bytes");

struct mm_set_channel_req {
    uint8_t  band;
    uint8_t  type;
    uint16_t prim20_freq;
    uint16_t center1_freq;
    uint16_t center2_freq;
    uint8_t  index;
    int8_t   tx_power;
};

struct mm_channel_survey_ind {
    uint16_t freq;
    int8_t   noise_dbm;
    uint32_t chan_time_ms;
    uint32_t chan_time_busy_ms;
};
static_assert(sizeof(struct mm_channel_survey_ind) == 12, "mm_task.o DWARF: mm_channel_survey_ind is 12 bytes");

#ifdef __cplusplus
}
#endif
