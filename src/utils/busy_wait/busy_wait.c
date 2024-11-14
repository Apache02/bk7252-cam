#include "utils/busy_wait.h"

static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
            ".syntax unified\n"
            "1: subs %0, #3\n"
            "bcs 1b\n"
            : "+l" (minimum_cycles) : : "cc", "memory"
            );
}

void busy_wait_us(unsigned int us) {
    busy_wait_at_least_cycles(us * (120000000 / 1000000));
}

void busy_wait_ms(unsigned int ms) {
    busy_wait_us(ms * 1000);
}

void busy_wait(unsigned int sec) {
    for (unsigned int i = 0; i < sec; i++) {
        busy_wait_us(1000000);
    }
}
