#ifndef _PLATFORM_RTC_H
#define _PLATFORM_RTC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Uptime in whole seconds since boot (1Hz tick, own timer channel).
uint32_t rtc_get_uptime(void);

// Establishes wall-clock time: unix_time is the current UTC epoch second.
void rtc_set_time(uint32_t unix_timestamp);

// Current wall-clock UTC time as unix epoch seconds since EPOCH_START by default,
// or since the last rtc_set_time() call.
uint32_t rtc_get_time(void);

struct rtc_datetime {
    uint16_t year;
    uint8_t  month;  // 1-12
    uint8_t  day;    // 1-31
    uint8_t  hour;   // 0-23
    uint8_t  minute; // 0-59
    uint8_t  second; // 0-59
};

// Breaks down a unix epoch second (e.g. from rtc_get_time()) into UTC calendar fields.
void rtc_time_to_datetime(uint32_t unix_time, struct rtc_datetime *out);

// Breaks down rtc_get_time() into UTC calendar fields.
void rtc_get_datetime(struct rtc_datetime *out);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_RTC_H
