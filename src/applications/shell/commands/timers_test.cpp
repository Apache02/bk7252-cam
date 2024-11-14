#include "commands.h"
#include <stdio.h>
#include "utils/busy_wait.h"
#include "hardware/timer.h"


#define TRACE(var)                                  printf(#var " = 0x%lx\r\n", var)
#define TRACE_CHANGE(var, new_value)                printf(#var ": 0x%lx -> 0x%lx\r\n", var, new_value)


static void timer_complete(int timer_num) {
    putchar('-');
    putchar('0' + timer_num);
}

void command_timers_test(Console &c) {
    for (int i = 0; i < 10; i++) {
        int timer_num = timer_create(32000 * 2, timer_complete, true);

        if (timer_num >= 0) {
            timer_start(timer_num);
            putchar('+');
            putchar('0' + timer_num);
        } else {
            putchar('!');
            busy_wait(1);
        }
    }
    printf("\r\n");
}

void command_timers_test2(Console &c) {
    printf("%llu\r\n", time());
    busy_wait(1);
    printf("%llu\r\n", time());

    for (int i = 0; i < 6; i++) {
        printf("timer #%d value = %d\r\n", i, timer_read(i));
    }
}
