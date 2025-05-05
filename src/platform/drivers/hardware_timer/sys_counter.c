#include "hardware/timer.h"
#include <sys/cdefs.h>


static uint32_t g_sys_counter = 0;

void sys_counter_tick(__unused int timer_num) {
    g_sys_counter++;
}

uint32_t sys_counter_get_count() {
    return g_sys_counter;
}
