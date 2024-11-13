#include "shell_handlers.h"
#include <stdio.h>
#include "hardware/wdt.h"

#include "shell_commands.h"


static void help(Console &c) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) {
            break;
        }

        printf("  %s\r\n", handlers[i].name);
    }
}

static void reboot(Console &c) {
    printf("\r\nreboot system\r\n");
    wdt_reboot(100);
}

const Console::Handler handlers[] = {
        {"help",       help},
        {"reboot",     reboot},
        {"echo",       command_echo},
        {"dump",       command_dump},
        {"dump32",     command_dump32},
        {"partitions", command_partitions},
        {"stack",      command_stack},
        // required at the end
        {nullptr,      nullptr},
};
