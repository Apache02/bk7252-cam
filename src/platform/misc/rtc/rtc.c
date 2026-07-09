#include "platform/rtc.h"
#include "hardware/timer.h"
#include "platform/cpu.h"
#include "platform/init.h"
#include <stdio.h>
#include <assert.h>


#define EPOCH_START (1767225600) // 2026-01-01 00:00:00 UTC


static volatile uint32_t g_uptime_seconds = 0;
static volatile uint32_t g_epoch_offset   = 0;

static void rtc_isr(__unused int timer_num) { g_uptime_seconds++; }

static void rtc_init(void) {
    int timer = timer_create_by_freq(1, &rtc_isr, false);
    assert(timer >= 0);
    timer_start(timer);
    rtc_set_time(EPOCH_START);
}

INIT_AT(rtc_init, 03);

uint32_t rtc_get_uptime(void) { return g_uptime_seconds; }

void rtc_set_time(uint32_t unix_time) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    g_epoch_offset = unix_time - g_uptime_seconds;
    GLOBAL_INT_RESTORE();
}

uint32_t rtc_get_time(void) { return g_epoch_offset + g_uptime_seconds; }
