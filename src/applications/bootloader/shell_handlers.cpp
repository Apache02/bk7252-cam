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
    {"help",        help,                          "print this help"},
    {"reboot",      command_reboot,                "perform reboot"},
    {"crc32",       command_crc32,                 "CRC32 of memory: <addr> <size>"},
    {"speed",       command_uart2_baudrate,        "set UART2 baud rate: <baud>"},

    {"go",          command_jump,                  "jump to address: <addr|app|iram|bootloader>"},
    {"loadi",       command_iram_load,             "load binary to RAM: <addr> <size> <checksum>"},
    {"loadx",       command_iram_xmodem,           "load XMODEM binary to RAM: <addr>"},

    {"buffer",      command_buffer,                "allocate RAM buffer for flash_write: <size>"},
    {"flash_write", command_flash_write,           "write RAM to flash: <ram_addr> <flash_addr> <size> <checksum>"},
    {"flash_dump",  command_flash_dump,            "hex dump flash: <addr> [size]"},
    {"flash_read",  command_flash_read_binary,     "read flash as binary stream: <addr> <size>"},
    {"flash_crc32", command_flash_crc32,           "CRC32 of flash: <addr> <size>"},
    {"flash",       command_flash,                 "flash <action: id|unprotect>"},

    {"partitions",  command_partitions,            "find and print partitions table"},
    {"chip_id",     command_chip_id,               "print chip and device IDs"},
    {"dump",        command_dump,                  "hex dump of memory: [addr]"},
    {"dump32",      command_dump32,                "32-bit word dump of memory: [addr]"},
    {"gpio",        command_gpio,                   nullptr},

    {nullptr, nullptr, nullptr},
};
