#include "hardware/time.h"

#define NXMAC_MONOTONIC_COUNTER_1_ADDR    0xC000011C
#define NXMAC_MONOTONIC_COUNTER_2_LO_ADDR 0xC0000120
#define NXMAC_MONOTONIC_COUNTER_2_HI_ADDR 0xC0000124

typedef volatile struct {
    uint32_t timeraw_hf;
    uint32_t timerawh;
    uint32_t timerawl;
} nxmac_timer_t;

#define hw_nxmac_counter ((volatile nxmac_timer_t *)NXMAC_MONOTONIC_COUNTER_1_ADDR)

uint32_t get_hf_counter() { return hw_nxmac_counter->timeraw_hf; }

uint32_t get_us_counter() { return hw_nxmac_counter->timerawh; }

absolute_time_t get_absolute_time() {
    // 64-bit counter split across two 32-bit MMIO words. Read high, low, high
    // again; if the high half changed, the low half wrapped between the two
    // reads — re-read the low half against the new high half.
    // Note: register named timerawl actually holds the HIGH 32 bits and
    // timerawh holds the LOW 32 bits (see *_HI/_LO addresses).
    uint32_t hi1, hi2, lo;
    do {
        hi1 = hw_nxmac_counter->timerawl;
        lo  = hw_nxmac_counter->timerawh;
        hi2 = hw_nxmac_counter->timerawl;
    } while (hi1 != hi2);

    absolute_time_t time;
    time.time_raw = ((uint64_t)hi2 << 32) | lo;
    return time;
}

uint64_t to_us_since_boot(absolute_time_t t) { return t.time_raw; }

uint64_t to_ms_since_boot(absolute_time_t t) { return t.time_raw / 1000; }

absolute_time_t delayed_by_us(absolute_time_t t, uint64_t us) {
    t.time_raw += us;
    return t;
}

absolute_time_t make_timeout_time_us(uint64_t us) { return delayed_by_us(get_absolute_time(), us); }

int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
    return (int64_t)(to_us_since_boot(to) - to_us_since_boot(from));
}
