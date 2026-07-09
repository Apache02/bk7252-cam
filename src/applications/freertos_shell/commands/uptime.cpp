#include <stdio.h>
#include "platform/rtc.h"

int command_uptime(__unused int argc, __unused const char *argv[]) {
    uint32_t total = rtc_get_uptime();
    int      days, hours, minutes, seconds;

    days    = total / (24 * 3600);
    seconds = total - (days * (24 * 3600));
    hours   = seconds / 3600;
    seconds -= hours * 3600;
    minutes = seconds / 60;
    seconds -= minutes * 60;

    if (days > 0) {
        printf("uptime: %d days %02d:%02d:%02d\r\n", days, hours, minutes, seconds);
    } else if (hours > 0) {
        printf("uptime: %d:%02d:%02d\r\n", hours, minutes, seconds);
    } else if (minutes > 0) {
        printf("uptime: %d:%02d\r\n", minutes, seconds);
    } else if (seconds == 1) {
        printf("uptime: %d second\r\n", seconds);
    } else {
        printf("uptime: %d seconds\r\n", seconds);
    }

    return 0;
}
