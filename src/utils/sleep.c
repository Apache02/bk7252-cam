#include "utils/sleep.h"

static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
    ".syntax unified\n"
    "1: subs %0, #3\n"
    "bcs 1b\n"
    : "+l" (minimum_cycles) : : "cc", "memory"
    );
}

void usleep(unsigned int us) {
    busy_wait_at_least_cycles(us * (180000000 / 1000000));
}

void msleep(unsigned int ms) {
    usleep(ms * 1000);
}

void sleep(unsigned int sec) {
    for (unsigned int i = 0; i < sec; i++) {
        usleep(1000000);
    }
}
