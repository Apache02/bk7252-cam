#pragma once

// APM, task 5 — AP mode. Compiled into the archive (NX_BEACONING was on), but
// this port never reaches it: the station path never calls APM.
//
// Ids come from the archive's apm_task.h. Only the four _REQ handlers are
// present in apm_task.o; the three _IND ids are sent by APM, never received by
// it, so they need no handler of their own.

#include "libip/ke.h"

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

#ifdef __cplusplus
}
#endif
