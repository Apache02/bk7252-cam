#include "utils/busy_wait.h"

// ARM968E-S (5-stage pipeline, Thumb): `subs` = 1 cycle, taken `bcs` = 3 cycles,
// so one loop iteration is 4 cycles — subtract 4 per iteration to match.
static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
        ".syntax unified\n"
        "1: subs %0, #4\n"
        "bcs 1b\n"
        : "+l" (minimum_cycles) : : "cc"
    );
}

void busy_wait_us(unsigned int us) {
    busy_wait_at_least_cycles((unsigned long) us * (120000000UL / 1000000UL));
}

void busy_wait_ms(unsigned int ms) {
    busy_wait_us(ms * 1000);
}

void busy_wait(unsigned int sec) {
    for (unsigned int i = 0; i < sec; i++) {
        busy_wait_us(1000000);
    }
}
