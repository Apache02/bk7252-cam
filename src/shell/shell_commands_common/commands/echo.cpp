#include "shell/commands_common.h"
#include <stdio.h>


int command_echo(int argc, const char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        const char *ptr = argv[i];
        while (*ptr != '\0') putchar(*ptr++);
    }
    putchar('\r');
    putchar('\n');

    return 0;
}
