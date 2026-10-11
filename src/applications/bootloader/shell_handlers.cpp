#include "shell_handlers.h"
#include "shell/commands_common.h"
#include "shell/commands_beken.h"
#include "shell/commands_iram.h"
#include <stdio.h>
#include "version.h"

static int help(__unused int argc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

static int version(__unused int argc, __unused const char *argv[]) {
    print_version();
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help", help, "print this help"},
    {"version", version, "print bootloader version and build time"},
    {"reboot", command_reboot, "perform reboot"},
    {"crc32", command_crc32, "CRC32 of memory: <addr> <size>"},
    {"speed", command_uart2_baudrate, "set UART2 baud rate: <baud>"},

    {"go", command_jump, "jump to address: <addr|app|iram|bootloader>"},
    {"loadi", command_iram_load, "load binary to RAM: <addr> <size> <checksum>"},
    {"loadx", command_iram_xmodem, "load XMODEM binary to RAM: <addr>"},

    {"buffer", command_buffer, "allocate RAM buffer for flash_write: <size>"},
    {"flash_write", command_flash_write, "write RAM to flash: <ram_addr> <flash_addr> <size> <checksum>"},
    {"flash_dump", command_flash_dump, "hex dump flash: <addr> [size]"},
    {"flash_read", command_flash_read_binary, "read flash as binary stream: <addr> <size>"},
    {"memory_read", command_memory_read_binary, "read memory as binary stream: <addr> <size>"},
    {"flash_crc32", command_flash_crc32, "CRC32 of flash: <addr> <size>"},
    {"flash", command_flash, "flash <action: id|unprotect>"},
    {"tlv", command_tlv, "decode TLV blob: part <name> | addr <addr> [size]"},

    {"partitions", command_partitions, "find and print partitions table"},
    {"chip_id", command_chip_id, "print chip and device IDs"},
    {"efuse", command_efuse, "print eFuse words, UID and MAC (read only)"},
    {"dump", command_dump, "hex dump of memory: [addr]"},
    {"dump32", command_dump32, "32-bit word dump of memory: [addr]"},
    {"gpio", command_gpio, "pin mode: <pin|all> out|in|in_pu|in_pd"},
    {"cpu_speed", command_cpu_speed, "measure CPU frequency: [iterations] [--use_timer]"},
    {"mclk", command_mclk, "show or set the main clock: [source [divider]]"},

    {nullptr, nullptr, nullptr},
};
