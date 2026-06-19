#include "shell_handlers.h"
#include "shell/commands_common.h"
#include "shell/commands_beken.h"
#include "shell/commands_iram.h"
#include "commands/commands.h"
#include <stdio.h>

static int help(__unused int intc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help", help, nullptr},
    {"echo", command_echo, nullptr},
    {"reboot", command_reboot, nullptr},
    {"crc32", command_crc32, nullptr},
    {"speed", command_uart2_baudrate, nullptr},

    {"chip_id", command_chip_id, nullptr},
    {"partitions", command_partitions, nullptr},
    {"dump", command_dump, nullptr},
    {"dump32", command_dump32, nullptr},
    {"flash_dump", command_flash_dump, nullptr},
    {"flash_read", command_flash_read_binary, nullptr},
    {"flash_write", command_flash_write, nullptr},
    {"flash_crc32", command_flash_crc32, nullptr},

    {"loadi", command_iram_load, nullptr},
    {"loadx", command_iram_xmodem, nullptr},
    {"go", command_jump, nullptr},

    {"cpu_speed", command_cpu_speed, nullptr},
    {"mclk", command_mclk, nullptr},

    // required at the end
    {nullptr, nullptr, nullptr},
};
