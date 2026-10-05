#pragma once

// SCANU, task 2 — scan sessions and join. This is what wifi_scan talks to; the
// sweep itself is run by SCAN below it.
//
// Ids come from the archive's scanu_task.h, cross-checked against scanu_task.o.
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

enum scanu_msg_tag {
    SCANU_START_REQ = KE_FIRST_MSG(TASK_SCANU),
    SCANU_START_CFM,
    SCANU_JOIN_REQ,
    SCANU_JOIN_CFM,
    SCANU_RESULT_IND,
    SCANU_FAST_REQ,
    SCANU_FAST_CFM,
};

// How many SSIDs one scan request can carry — not how long an SSID may be.
#define SCANU_SSID_SLOTS 2

// 336 bytes per scanu_task.o's DWARF, which also confirms the member list ends
// at no_cck.
struct scanu_start_req {
    struct scan_chan_tag chan[SCAN_CHANNEL_MAX];
    struct mac_ssid      ssid[SCANU_SSID_SLOTS];
    struct mac_addr      bssid;
    uint32_t             add_ies;
    uint16_t             add_ie_len;
    uint8_t              vif_idx;
    uint8_t              chan_cnt;
    uint8_t              ssid_cnt;
    bool                 no_cck;
};
static_assert(sizeof(struct scanu_start_req) == 336, "scanu_task.o DWARF: scanu_start_req is 336 bytes");

// One received beacon or probe response, frame and all. `payload` is declared
// UINT32[], so it is 4-aligned and there are two padding bytes after `rssi`.
struct scanu_result_ind {
    uint16_t length; // 802.11 frame length
    uint16_t framectrl;
    uint16_t center_freq; // MHz the frame arrived on
    uint8_t  band;
    uint8_t  sta_idx;
    uint8_t  inst_nbr;
    int8_t   rssi;
    uint32_t payload[];
};
static_assert(offsetof(struct scanu_result_ind, payload) == 12, "scanu_task.o DWARF");

#ifdef __cplusplus
}
#endif
