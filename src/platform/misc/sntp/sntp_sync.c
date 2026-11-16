#include "platform/sntp.h"

#include <stdint.h>
#include <stdio.h>

#include "lwip/apps/sntp.h"
#include "lwip/tcpip.h"

#include "device/soft_rtc.h"

#define NTP_SERVER "pool.ntp.org"

// Called by the lwIP SNTP client (see SNTP_SET_SYSTEM_TIME in lwipopts.h) on every good reply.
void sntp_set_system_time(uint32_t unix_time) {
    rtc_set_time(unix_time);
    printf("[time] synced from %s\r\n", NTP_SERVER);
}

void sntp_sync_start(void) {
    LOCK_TCPIP_CORE();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, NTP_SERVER);
    sntp_init();
    UNLOCK_TCPIP_CORE();
}
