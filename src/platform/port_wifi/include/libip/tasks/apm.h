#pragma once

// APM, task 5 — AP mode. Compiled into the archive (NX_BEACONING was on). The
// archive only beacons: probe, authentication and association handling belong
// to the host, see api/ap.cpp.
//
// Ids come from the archive's apm_task.h. Only the four _REQ handlers are
// present in apm_task.o; the three _IND ids are sent by APM, never received by
// it, so they need no handler of their own.
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

enum apm_msg_tag {
    APM_START_REQ = KE_FIRST_MSG(TASK_APM),
    APM_START_CFM,
    APM_STOP_REQ,
    APM_STOP_CFM,
    APM_START_CAC_REQ,
    APM_START_CAC_CFM,
    APM_STOP_CAC_REQ,
    APM_STOP_CAC_CFM,
    APM_ASSOC_IND,
    APM_DEASSOC_IND,
    APM_ASSOC_FAILED_IND,
};

struct apm_start_req {
    struct mac_rateset   basic_rates;         // 0
    struct scan_chan_tag chan;                // 14
    uint32_t             center_freq1;        // 20
    uint32_t             center_freq2;        // 24
    uint8_t              ch_width;            // 28
    uint32_t             bcn_addr;            // 32, host address of the beacon template
    uint16_t             bcn_len;             // 36
    uint16_t             tim_oft;             // 38, offset of the TIM element in the template
    uint16_t             bcn_int;             // 40, TU
    uint32_t             flags;               // 44
    uint16_t             ctrl_port_ethertype; // 48, network byte order
    uint8_t              tim_len;             // 50, whole TIM element
    uint8_t              vif_idx;             // 51
};

struct apm_start_cfm {
    uint8_t status;
    uint8_t vif_idx;
    uint8_t ch_idx;
    uint8_t bcmc_idx; // station index the LMAC uses for broadcast and multicast
};

struct apm_stop_req {
    uint8_t vif_idx;
};

struct apm_stop_cfm {
    uint8_t status;
};

struct apm_assoc_ind {
    struct mac_addr mac;
};

static_assert(sizeof(struct apm_start_req) == 52, "apm_task.o DWARF");
static_assert(offsetof(struct apm_start_req, chan) == 14, "");
static_assert(offsetof(struct apm_start_req, center_freq1) == 20, "");
static_assert(offsetof(struct apm_start_req, ch_width) == 28, "");
static_assert(offsetof(struct apm_start_req, bcn_addr) == 32, "");
static_assert(offsetof(struct apm_start_req, bcn_len) == 36, "");
static_assert(offsetof(struct apm_start_req, tim_oft) == 38, "");
static_assert(offsetof(struct apm_start_req, bcn_int) == 40, "");
static_assert(offsetof(struct apm_start_req, flags) == 44, "");
static_assert(offsetof(struct apm_start_req, ctrl_port_ethertype) == 48, "");
static_assert(offsetof(struct apm_start_req, tim_len) == 50, "");
static_assert(offsetof(struct apm_start_req, vif_idx) == 51, "");
static_assert(sizeof(struct apm_start_cfm) == 4, "apm_task.o DWARF");
static_assert(sizeof(struct apm_stop_req) == 1, "apm_task.o DWARF");
static_assert(sizeof(struct apm_assoc_ind) == 6, "me_task.o DWARF");

#ifdef __cplusplus
}
#endif
