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
void rtc_set_time(uint32_t unix_time);

// Current wall-clock UTC time as unix epoch seconds. Returns 0 if !rtc_is_set().
uint32_t rtc_get_time(void);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_RTC_H
