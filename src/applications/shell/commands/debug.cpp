#include "commands.h"
#include "shell/Parser.h"
#include <stdio.h>

#include "utils/busy_wait.h"
#include "hardware/wdt.h"


int command_reboot(int argc, const char *argv[]) {
    int delay = argc == 2
                    ? take_int(argv[1]).ok_or(0)
                    : 0;
    if (delay > 9) delay = 9;

    if (delay > 0) {
        printf("rebooting ... %d", delay);
        while (delay > 0) {
            printf("\b%d", delay);

            busy_wait(1);
            delay--;
        }
    }


    printf("\r\nreboot system\r\n");
    busy_wait_ms(100);

    wdt_reboot(100);

    return 0;
}
