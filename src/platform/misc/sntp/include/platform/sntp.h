#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Keeps the soft RTC in step with an NTP server. Needs net_init() done first.
// It asks right away and then once an hour. While there is no network it retries with a growing pause.
void sntp_sync_start(void);

#ifdef __cplusplus
}
#endif
