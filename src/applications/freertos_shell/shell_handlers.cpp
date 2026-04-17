#include "shell_handlers.h"
#include <stdio.h>
#include "hardware/wdt.h"

#include "shell/commands_common.h"
#include "shell/commands_freertos.h"
#include "shell/commands_beken.h"
#include "shell/commands_iram.h"
#include "shell/Shell.h"


static int help(__unused int intc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

extern int command_free(int argc, const char *argv[]);

extern int command_blink(int argc, const char *argv[]);

extern int command_test_net(int argc, const char *argv[]);

const Shell::Handler shell_handlers[] = {
    {"help", help, nullptr},
    {"reboot", command_reboot, nullptr},
    {"crc32", command_crc32, nullptr},
    {"echo", command_echo, nullptr},
    {"dump", command_dump, nullptr},
    {"dump32", command_dump32, nullptr},
    {"chip_id", command_chip_id, nullptr},
    {"partitions", command_partitions, nullptr},
    {"efuse", command_efuse, nullptr},
    {"stack", command_stack, nullptr},
    {"tasks", command_tasks, nullptr},
    {"free", command_free, nullptr},
    {"blink", command_blink, nullptr},
    {"flash_dump", command_flash_dump<uint8_t>, nullptr},
    {"flash_dump32", command_flash_dump<uint32_t>, nullptr},
    {"random_test", command_random_test, nullptr},
    {"timer_delay", command_time_delay, nullptr},
    {"uptime", command_uptime, nullptr},
    {"cpu_speed", command_cpu_speed, nullptr},
    {"mclk", command_mclk, nullptr},
    {"loadi", command_iram_load, nullptr},
    {"loadx", command_iram_xmodem, nullptr},
    {"go", command_iram_jump, nullptr},
    {"test_net", command_test_net, nullptr},
    {"probe_ram", command_probe_ram, nullptr},
    // required at the end
    {nullptr, nullptr, nullptr},
};
