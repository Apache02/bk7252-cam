#include "commands.h"
#include <stdio.h>
#include "utils/sleep.h"
#include "hardware/timer.h"


#define TRACE(var)                                  printf(#var " = 0x%lx\r\n", var)
#define TRACE_CHANGE(var, new_value)                printf(#var ": 0x%lx -> 0x%lx\r\n", var, new_value)


void command_timers(Console &c) {
    for (int i = 0; i < 10; i++) {
        int timer_num = timer_create(true, 26000 * i, 1, [](int t) {
            printf("timer #%d complete\r\n", t);
        });

        if (timer_num >= 0) {
            printf("timer #%d created\r\n", timer_num);
        } else {
            sleep(1);
        }
    }
}

void command_timer_test(Console &c) {
    printf("%llu\r\n", time_us_64());
    sleep(1);
    printf("%llu\r\n", time_us_64());
}