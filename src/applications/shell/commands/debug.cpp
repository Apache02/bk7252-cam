#include "commands.h"
#include <stdio.h>

#include "utils/busy_wait.h"
#include "hardware/wdt.h"
#include "hardware/sctrl.h"


void command_reboot(Console &c) {
    int delay = c.packet.take_int().ok_or(0);
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
}

void command_chip_id(Console &c) {
    printf("chip id: 0x%lx\r\n", chip_id());
    printf(" dev id: 0x%lx\r\n", device_id());
}

