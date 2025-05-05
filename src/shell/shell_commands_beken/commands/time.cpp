#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/time.h"
#include "hardware/sys_counter.h"
#include "hardware/cpu.h"


static char * sprint_time(char * buf, uint64_t us) {
    unsigned int ms = us / 1000;
    unsigned int s = ms / 1000;
    ms -= s * 1000;
    sprintf(buf, "%d.%03d", s, ms);
    return buf;
}

void command_time_delay(Console &c) {
    uint32_t seconds = c.packet.take_int().ok_or(0);
    if (seconds == 0 || seconds > 3600) {
        printf(COLOR_RED("Error: seconds must be in range [1-3600]") "\r\n");
        return;
    }

    absolute_time_t start = get_absolute_time();
    char sTime[16];
    printf("start at %s\r\n", sprint_time(sTime, to_us_since_boot(start)));
    for (uint64_t diff = 0; diff < seconds * 1000000;) {
        diff = absolute_time_diff_us(start, get_absolute_time());

        // update value
        putchar('\r');
        printf("%s", sprint_time(sTime, diff));
    }
    printf("\r\n");
    printf("done at %s\r\n", sprint_time(sTime, to_us_since_boot(get_absolute_time())));
}

static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
            ".syntax unified\n"
            "1: subs %0, #3\n"
            "bcs 1b\n"
            : "+l" (minimum_cycles) : : "cc", "memory"
            );
}

void command_uptime(Console &c) {
    uint32_t total = sys_counter_get_count();
    int days, hours, minutes, seconds;

    days = total / (24 * 3600);
    seconds = total - (days * (24 * 3600));
    hours = seconds / 3600;
    seconds -= hours * 3600;
    minutes = seconds / 60;
    seconds -= minutes * 60;

    if (days > 0) {
        printf("uptime: %d days %02d:%02d:%02d\r\n", days, hours, minutes, seconds);
    } else if (hours > 0) {
        printf("uptime: %d:%02d:%02d\r\n", hours, minutes, seconds);
    } else if (minutes > 0) {
        printf("uptime: %d:%02d\r\n", minutes, seconds);
    } else if (seconds == 1) {
        printf("uptime: %d second\r\n", seconds);
    } else {
        printf("uptime: %d seconds\r\n", seconds);
    }
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
