#include "commands.h"
#include <stdio.h>

#include "utils/sleep.h"
#include "hardware/wdt.h"
#include "hardware/sctrl.h"


void command_reboot(Console &c) {
    int delay = c.packet.take_int().ok_or(0);
    if (delay > 9) delay = 9;

    if (delay > 0) {

        printf("rebooting ... %d", delay);
        while (delay > 0) {
            printf("\b%d", delay);

            sleep(1);
            delay--;
        }
    }


    printf("\r\nreboot system\r\n");
    msleep(100);

    wdt_set(10);
    wdt_up();

    while (1);
}

#define DUMP_DEFAULT_ADDRESS        0x00000000

void command_dump(Console &c) {
    auto addr = c.packet.take_int().ok_or(DUMP_DEFAULT_ADDRESS);
    printf("addr 0x%08x\r\n", addr);

    if (addr & 3) {
        printf("%s\r\n", COLOR_RED("addr must be aligned to 4"));
        return;
    }

    for (size_t i = 0; i < (16 * 16);) {
        printf("%02x ", (*((volatile uint8_t *) (addr + i))));
        i += sizeof(uint8_t);
        if (i % 16 == 0) {
            printf("\r\n");
        }
    }
}

void command_dump32(Console &c) {
    auto addr = c.packet.take_int().ok_or(DUMP_DEFAULT_ADDRESS);
    printf("addr 0x%08x\r\n", addr);

    if (addr & 3) {
        printf("%s\r\n", COLOR_RED("addr must be aligned to 4"));
        return;
    }

    for (size_t i = 0; i < (16 * 16);) {
        printf("%08lx ", (*((volatile uint32_t *) (addr + i))));
        i += sizeof(uint32_t);
        if (i % 16 == 0) {
            printf("\r\n");
        }
    }
}

void command_chip_id(Console &c) {
    printf("chip id: 0x%lx\r\n", chip_id());
    printf(" dev id: 0x%lx\r\n", device_id());
}

