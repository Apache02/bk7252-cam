#include "shell/commands_beken.h"
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include <stdio.h>
#include "hardware/time.h"
#include "hardware/sys_counter.h"


static char *sprint_time(char *buf, uint64_t us) {
    unsigned int ms = us / 1000;
    unsigned int s = ms / 1000;
    ms -= s * 1000;
    sprintf(buf, "%d.%03d", s, ms);
    return buf;
}

int command_time_delay(int argc, const char *argv[]) {
    uint32_t seconds = argc == 2
                           ? take_int(argv[1]).ok_or(0)
                           : 0;
    if (seconds == 0 || seconds > 3600) {
        printf(COLOR_RED("Error: seconds must be in range [1-3600]") "\r\n");
        return 1;
    }

    absolute_time_t start = get_absolute_time();
    char sTime[16];
    printf("start at %s\r\n", sprint_time(sTime, to_us_since_boot(start)));
    for (uint64_t diff = 0; diff < seconds * 1000000;) {
        diff = absolute_time_diff_us(start, get_absolute_time());

        // update value
        putchar('\r');
        printf("%s", sprint_time(sTime, diff));
    }
    printf("\r\n");
    printf("done at %s\r\n", sprint_time(sTime, to_us_since_boot(get_absolute_time())));

    return 0;
}

int command_uptime(__unused int argc, __unused const char *argv[]) {
    uint32_t total = sys_counter_get_count();
    int days, hours, minutes, seconds;

    days = total / (24 * 3600);
    seconds = total - (days * (24 * 3600));
    hours = seconds / 3600;
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
