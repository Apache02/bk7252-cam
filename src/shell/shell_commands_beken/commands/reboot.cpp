#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/wdt.h"

int command_reboot(__unused int argc, __unused const char *argv[]) {
    printf("\r\nreboot system\r\n");
    wdt_reboot(100);
    return 0;
}
