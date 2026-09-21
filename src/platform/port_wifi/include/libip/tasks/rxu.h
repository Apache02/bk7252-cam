#pragma once

// RXU, task 7 — dispatches incoming management and null-data frames.
//
// One id is not one recipient here: RXU_MGT_IND is handled by three tasks at
// once. SCANU, SM and BAM each define their own rxu_mgt_ind_handler and inspect
// the same frame for whatever they individually care about.

#include "libip/ke.h"

#ifdef __cplusplus
extern "C" {
#endif

enum rxu_msg_tag {
    RXU_MGT_IND = KE_FIRST_MSG(TASK_RXU),
    RXU_NULL_DATA,
};

#ifdef __cplusplus
}
#endif
