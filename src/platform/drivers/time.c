#include "drivers/time.h"
#include "stdint.h"


#define NXMAC_MONOTONIC_COUNTER_1_ADDR      0xC000011C
#define NXMAC_MONOTONIC_COUNTER_2_LO_ADDR   0xC0000120
#define NXMAC_MONOTONIC_COUNTER_2_HI_ADDR   0xC0000124

typedef struct {
    uint32_t timeraw_hf;
    uint32_t timerawh;
    uint32_t timerawl;
} nxmac_timer_t;

#define hw_nxmac_timer    ((volatile nxmac_timer_t *) NXMAC_MONOTONIC_COUNTER_1_ADDR)


uint32_t get_hf_counter() {
    return hw_nxmac_timer->timeraw_hf;
}

absolute_time_t get_absolute_time() {
    absolute_time_t time;
    time.time_raw = (((uint64_t) hw_nxmac_timer->timerawl) << 32) | hw_nxmac_timer->timerawh;
    return time;
}

uint64_t to_us_since_boot(absolute_time_t t) {
    return t.time_raw;
}

uint64_t to_ms_since_boot(absolute_time_t t) {
    return t.time_raw / 1000;
}

absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) {
    t.time_raw += us;
    return t;
}

absolute_time_t make_timeout_time_us(uint64_t us) {
    return delayed_by_us(get_absolute_time(), us);
}

int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
    return (int64_t) (to_us_since_boot(to) - to_us_since_boot(from));
}
