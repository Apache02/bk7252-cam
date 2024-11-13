#include "shell_handlers.h"
#include <stdio.h>
#include "hardware/wdt.h"


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
        {"help",   help},
        {"reboot", reboot},
        // required at the end
        {nullptr,  nullptr},
};
