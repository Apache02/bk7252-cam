#include "hardware/time.h"

#include "rwnx/mac_core.h"

uint32_t get_hf_counter() { return hw_mac_core->monotonic_counter_1; }

uint32_t get_us_counter() { return hw_mac_core->monotonic_counter_2_lo; }

absolute_time_t get_absolute_time() {
    // 48-bit counter split across two MMIO words. Read high, low, high again;
    // if the high half changed, the low half wrapped between the two reads —
    // re-read the low half against the new high half.
    uint32_t hi1, hi2, lo;
    do {
        hi1 = hw_mac_core->monotonic_counter_2_hi.v;
        lo  = hw_mac_core->monotonic_counter_2_lo;
        hi2 = hw_mac_core->monotonic_counter_2_hi.v;
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
