#include "commands.h"
#include <stdio.h>
#include "utils/sleep.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))

void command_watch_reg(Console &c) {
    size_t count = c.packet.take_int().ok_or(10000);

    uint32_t *regs[16];
    size_t regs_count = 0;
    for (int r = 0; r < count_of(regs); r++) {
        regs[r] = (uint32_t *) c.packet.take_int().ok_or(0);
        if (!regs[r]) {
            regs_count = r;
            break;
        }
    }

    if (!regs_count) {
        printf(COLOR_RED("Address not defined") "\r\n");
        return;
    }

    // print header, registers addresses
    printf("\r\n");
    for (int r = 0; r < regs_count; r++) {
        printf("0x%08lx ", (uint32_t) regs[r]);
    }
    printf("\r\n");

    // print values
    for (size_t i = 0; i < count; i++) {
        for (int r = 0; r < regs_count; r++) {
            printf("\b\b\b\b\b\b\b\b\b\b\b");
        }
        for (int r = 0; r < regs_count; r++) {
            printf("0x%08lx ", *regs[r]);
        }

        usleep(10);
    }

    printf("\r\n\r\n");
}
