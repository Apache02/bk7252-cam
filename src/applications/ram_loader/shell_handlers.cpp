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
    {"loadi", command_iram_load, nullptr},
    {"loadx", command_iram_xmodem, nullptr},
    {"go", command_iram_jump, nullptr},

    // required at the end
    {nullptr, nullptr, nullptr},
};
