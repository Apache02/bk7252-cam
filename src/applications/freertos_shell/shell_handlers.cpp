#include "shell_handlers.h"
#include <stdio.h>
#include "hardware/wdt.h"

#include "shell/commands_common.h"
#include "shell/commands_beken.h"


static void help(__unused Console &c) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) {
            break;
        }

        printf("  %s\r\n", handlers[i].name);
    }
}

static void reboot(__unused Console &c) {
    printf("\r\nreboot system\r\n");
    wdt_reboot(100);
}

extern void command_tasks(Console &);

extern void command_free(Console &);

extern void command_blink(Console &c);

const Console::Handler handlers[] = {
        {"help",       help},
        {"reboot",     reboot},
        {"echo",       command_echo},
        {"dump",       command_dump},
        {"dump32",     command_dump32},
        {"partitions", command_partitions},
        {"efuse",      command_efuse},
        {"stack",      command_stack},
        {"tasks",      command_tasks},
        {"free",       command_free},
        {"blink",      command_blink},
        // required at the end
        {nullptr,      nullptr},
};
