#pragma once

// BAM, task 6 — Block Ack agreement tracking.
//
// Both ids are timeouts. BAM learns about the ADDBA/DELBA action frames that
// actually drive it through its own rxu_mgt_ind_handler, not through either
// message here.

#include "libip/ke.h"

#ifdef __cplusplus
extern "C" {
#endif

enum bam_msg_tag {
    BAM_ADD_BA_RSP_TIMEOUT_IND = KE_FIRST_MSG(TASK_BAM),
    BAM_INACTIVITY_TIMEOUT_IND,
};

#ifdef __cplusplus
}
#endif
