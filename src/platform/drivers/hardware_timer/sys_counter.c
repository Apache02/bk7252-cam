#include "hardware/timer.h"
#include <sys/cdefs.h>


static uint32_t g_sys_counter = 0;

void sys_counter_tick(__unused int timer_num) {
    g_sys_counter++;
}

uint64_t time_us_64() {
    return g_sys_counter;
}
