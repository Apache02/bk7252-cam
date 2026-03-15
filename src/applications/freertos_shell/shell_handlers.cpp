#include "shell_handlers.h"
#include <stdio.h>
#include "hardware/wdt.h"

#include "shell/commands_common.h"
#include "shell/commands_freertos.h"
#include "shell/commands_beken.h"
#include "shell/Shell.h"


static int help(__unused int intc, __unused const char *argv[]) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!shell_handlers[i].name || !shell_handlers[i].handler) break;

        if (shell_handlers[i].description) {
            printf("  %-16s %s\r\n", shell_handlers[i].name, shell_handlers[i].description);
        } else {
            printf("  %s\r\n", shell_handlers[i].name);
        }
    }
    return 0;
}

static int reboot(__unused int argc, __unused const char *argv[]) {
    printf("\r\nreboot system\r\n");
    wdt_reboot(100);
    return 0;
}

extern int command_free(int argc, const char *argv[]);

extern int command_blink(int argc, const char *argv[]);

const Shell::Handler shell_handlers[] = {
    {"help", help, ""},
    {"reboot", reboot, ""},
    {"echo", command_echo, ""},
    {"dump", command_dump, ""},
    {"dump32", command_dump32, ""},
    {"chip_id", command_chip_id, ""},
    {"partitions", command_partitions, ""},
    {"efuse", command_efuse, ""},
    {"stack", command_stack, ""},
    {"tasks", command_tasks, ""},
    {"free", command_free, ""},
    {"blink", command_blink, ""},
    {"flash_dump", command_flash_dump<uint8_t>, ""},
    {"flash_dump32", command_flash_dump<uint32_t>, ""},
    {"random_test", command_random_test, ""},
    {"timer_delay", command_time_delay, ""},
    {"uptime", command_uptime, ""},
    {"cpu_speed", command_cpu_speed, ""},
    // required at the end
    {nullptr, nullptr},
};
