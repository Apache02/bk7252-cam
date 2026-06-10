#include "shell_handlers.h"
#include "shell/commands_common.h"
#include "shell/commands_beken.h"
#include "shell/commands_iram.h"
#include <stdio.h>

static int help(__unused int argc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help",        help,                          nullptr},
    {"reboot",      command_reboot,                nullptr},
    {"crc32",       command_crc32,                 nullptr},
    {"speed",       command_uart2_baudrate,        nullptr},

    {"go",          command_jump,                  nullptr},
    {"go_app",      command_jump_app,              nullptr},
    {"loadi",       command_iram_load,             nullptr},
    {"loadx",       command_iram_xmodem,           nullptr},

    {"buffer",      command_buffer,                nullptr},
    {"flash_write", command_flash_write,           nullptr},
    {"flash_dump",  command_flash_dump,            nullptr},
    {"flash_read",  command_flash_read_binary,     nullptr},
    {"flash_crc32", command_flash_crc32,           nullptr},

    {"partitions",  command_partitions,            nullptr},
    {"chip_id",     command_chip_id,               nullptr},

    {nullptr, nullptr, nullptr},
};
