#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/time.h"
#include "hardware/cpu.h"


void command_time_delay(Console &c) {
    uint32_t seconds = c.packet.take_int().ok_or(0);
    if (seconds == 0 || seconds > 3600) {
        printf(COLOR_RED("Error: seconds must be in range [1-3600]") "\r\n");
        return;
    }

    absolute_time_t start = get_absolute_time();
    printf("start at %f\r\n", to_us_since_boot(start) / 1000000.0);
    for (uint64_t diff = 0; diff < seconds * 1000;) {
        diff = absolute_time_diff_us(start, get_absolute_time()) / 1000;

        // update value
        for (int i = 0; i < 10; i++) putchar('\b');
        printf(" %.03f", (float) diff / 1000.0);
    }
    printf("\r\ndone.\r\n");
}

static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
            ".syntax unified\n"
            "1: subs %0, #3\n"
            "bcs 1b\n"
            : "+l" (minimum_cycles) : : "cc", "memory"
            );
}

void command_cpu_speed(Console &c) {
    int count = c.packet.take_int().ok_or(1000000);

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    absolute_time_t start = get_absolute_time();
    busy_wait_at_least_cycles(count);
    int64_t us_spend = absolute_time_diff_us(start, get_absolute_time());
    GLOBAL_INT_RESTORE();

    printf("Instructions: %d\r\n", count);
    printf("It took %lu us\r\n", (uint32_t) us_spend);
    printf("CPU freq: %lu Hz\r\n", (uint32_t) (count / (us_spend / 1000000.0)));
}
