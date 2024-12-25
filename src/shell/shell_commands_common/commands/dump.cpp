#include "shell/commands_common.h"
#include <stdio.h>


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
