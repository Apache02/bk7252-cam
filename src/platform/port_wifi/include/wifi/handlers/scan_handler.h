#pragma once

// What one LMAC indication does to a scan session: the seam that lets
// rwnx_intf.c, which is C, drive a session written in C++. All three run in the
// kmsg task and do nothing when no scan is in flight. Only the message
// dispatcher calls these; wifi/scan.h is what an application uses.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// SCANU_RESULT_IND — one beacon or probe response.
void scan_ind_add_handler(const void *param, uint16_t param_len);

// MM_CHANNEL_SURVEY_IND — one channel's time-on-air statistics.
void scan_survey_add_handler(const void *param, uint16_t param_len);

// SCANU_START_CFM — the sweep is over. Resolves the promise.
void scan_finish_handler(void);

#ifdef __cplusplus
}
#endif
