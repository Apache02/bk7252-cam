#ifndef _TIME_H_
#define _TIME_H_

#include "stdint.h"

typedef struct {
    uint64_t time_raw;
} absolute_time_t;


#ifdef __cplusplus
extern "C" {
#endif

uint32_t get_hf_counter();

absolute_time_t get_absolute_time();

uint64_t to_us_since_boot(absolute_time_t t);

uint64_t to_ms_since_boot(absolute_time_t t);

absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us);

absolute_time_t make_timeout_time_us(uint64_t us);

int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to);

#ifdef __cplusplus
}
#endif
#endif // _TIME_H_
