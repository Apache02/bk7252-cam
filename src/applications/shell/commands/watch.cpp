#include "commands.h"
#include "shell/Parser.h"
#include "shell/console_colors.h"
#include <stdio.h>
#include <stdint.h>
#include "utils/busy_wait.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))

int command_watch_reg(int argc, const char *argv[]) {
    size_t count = argc > 1
                       ? take_int(argv[1]).ok_or(10000)
                       : 10000;

    uint32_t *regs[16];
    size_t regs_count = 0;
    for (int i = 2, r = 0; i < argc && r < static_cast<int>(count_of(regs)); i++, r++) {
        regs[r] = reinterpret_cast<uint32_t *>(take_int(argv[i]).ok_or(0));
        regs_count++;
    }

    if (!regs_count) {
        printf(COLOR_RED("Address not defined") "\r\n");
        return 1;
    }

    // print header, registers addresses
    printf("\r\n");
    for (unsigned int r = 0; r < regs_count; r++) {
        printf("0x%08lx ", (uint32_t) regs[r]);
    }
    printf("\r\n");

    // print values
    for (size_t i = 0; i < count; i++) {
        putchar('\r');
        for (unsigned int r = 0; r < regs_count; r++) {
            printf("0x%08lx ", *regs[r]);
        }

        busy_wait_us(10);
    }

    printf("\r\n\r\n");

    return 0;
}
