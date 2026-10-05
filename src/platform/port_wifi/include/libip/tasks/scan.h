#pragma once

// SCAN, task 1 — the LMAC's own channel-by-channel sweep. Driven by SCANU
// rather than by the host, with one exception: cancelling.
//
// Ids come from the archive's scan_task.h; the channel description below comes
// from the scan module's own scan.h next to it, and lives here because every
// task that names a channel reaches it through this one.

#include <stdint.h>

#include "libip/ke.h"

#ifdef __cplusplus
extern "C" {
#endif

enum scan_msg_tag {
    SCAN_START_REQ = KE_FIRST_MSG(TASK_SCAN),
    SCAN_START_CFM,
    SCAN_DONE_IND,
    // The only way to end a sweep early. scan_cancel_req_handler() takes no
    // parameters: it raises a cancel flag in the LMAC's scan state, the sweep
    // stops at the channel it is on, and the usual SCAN_DONE_IND ->
    // scanu_scan_next() -> SCANU_START_CFM chain closes the session as if it
    // had finished.
    SCAN_CANCEL_REQ,
    SCAN_CANCEL_CFM,

    SCAN_TIMER, // internal — a kernel timer message
};

#define SCAN_CHANNEL_2G4 14
#define SCAN_CHANNEL_5G  28
#define SCAN_CHANNEL_MAX (SCAN_CHANNEL_2G4 + SCAN_CHANNEL_5G)

struct scan_chan_tag {
    uint16_t freq;
    uint8_t  band;
    uint8_t  flags;
    int8_t   tx_power;
};

// Bit 0 of scan_chan_tag.flags, and named for something it does not do here: it
// only selects 110 ms of channel time over 90 ms.
//
// scan.o::scan_set_channel_request tests it and passes chan_scan_req() either
// 0x15f90 or 0x1adb0 us; mm_scan_channel_start_ind_handler meanwhile sends probe
// requests in a straight line without ever reading the flag, and those are the
// archive's only two callers of scan_probe_req_tx().
#define SCAN_PASSIVE_BIT  (1u << 0)
#define SCAN_DISABLED_BIT (1u << 31)

#ifdef __cplusplus
}
#endif
