#include "shell/commands_platform.h"
#include "device/soft_rtc.h"
#include <stdio.h>
#include <string.h>

static const char *const WEEKDAYS[] = {"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"};
static const char *const MONTHS[]   = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                       "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

static void print_date(void) {
    uint32_t         now = rtc_get_time();
    struct rtc_datetime dt;
    rtc_time_to_datetime(now, &dt);
    // 1970-01-01 was a Thursday.
    printf("%s %s %2u %02u:%02u:%02u UTC %u\r\n", WEEKDAYS[(now / 86400) % 7], MONTHS[dt.month - 1], dt.day,
           dt.hour, dt.minute, dt.second, dt.year);
}

static int set_date(int argc, const char *argv[]) {
    char text[32] = "";
    for (int i = 2; i < argc; i++) {
        if (strlen(text) + strlen(argv[i]) + 2 > sizeof(text)) {
            break;
        }
        if (i > 2) {
            strcat(text, " ");
        }
        strcat(text, argv[i]);
    }

    unsigned y, mo, d, h, mi, s;
    if (sscanf(text, "%u-%u-%u %u:%u:%u", &y, &mo, &d, &h, &mi, &s) != 6 || y < 1970 || y > 2105 || mo < 1 ||
        mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 59) {
        printf("usage: date -s YYYY-MM-DD HH:MM:SS (UTC)\r\n");
        return 1;
    }

    struct rtc_datetime dt = {(uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
    rtc_set_time(rtc_datetime_to_time(&dt));
    print_date();
    return 0;
}

int command_date(int argc, const char *argv[]) {
    if (argc == 1) {
        print_date();
        return 0;
    }
    if (strcmp(argv[1], "-s") == 0) {
        return set_date(argc, argv);
    }
    if (strcmp(argv[1], "+%s") == 0) {
        printf("%u\r\n", (unsigned)rtc_get_time());
        return 0;
    }
    printf("usage: date [-s YYYY-MM-DD HH:MM:SS | +%%s]\r\n");
    return 1;
}
