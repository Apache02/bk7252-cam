#include <stdint.h>

static inline void busy_wait_at_least_cycles(uint32_t minimum_cycles) {
    __asm volatile(
    ".syntax unified\n"
    "1: subs %0, #3\n"
    "bcs 1b\n"
    : "+l" (minimum_cycles) : : "cc", "memory"
    );
}

int main() {
    for (;;) {
        busy_wait_at_least_cycles(1000);
    }

    return 0;
}
