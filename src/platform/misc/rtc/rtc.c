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

void rtc_set_time(uint32_t unix_timestamp) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    g_epoch_offset = unix_timestamp - g_uptime_seconds;
    GLOBAL_INT_RESTORE();
}

uint32_t rtc_get_time(void) { return g_epoch_offset + g_uptime_seconds; }

// Howard Hinnant's civil_from_days, adapted to 32-bit unsigned arithmetic
// (valid for unix_time in [0, ~2106], well past this chip's epoch range).
void rtc_time_to_datetime(uint32_t unix_time, struct rtc_datetime *out) {
    uint32_t days = unix_time / 86400;
    uint32_t secs = unix_time % 86400;
    out->hour   = secs / 3600;
    out->minute = (secs % 3600) / 60;
    out->second = secs % 60;

    uint32_t z   = days + 719468;
    uint32_t era = z / 146097;
    uint32_t doe = z - era * 146097;
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    uint32_t y   = yoe + era * 400;
    uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp  = (5 * doy + 2) / 153;
    uint32_t d   = doy - (153 * mp + 2) / 5 + 1;
    uint32_t m   = mp + (mp < 10 ? 3 : -9);

    out->year  = y + (m <= 2);
    out->month = m;
    out->day   = d;
}

void rtc_get_datetime(struct rtc_datetime *out) { rtc_time_to_datetime(rtc_get_time(), out); }
