#include <stdio.h>
#include <stdint.h>
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include "utils/crc32.h"


int command_crc32(int argc, const char *argv[]) {
    if (argc != 3) {
        printf(COLOR_RED("Usage: crc32 0x<addr> <size>") "\r\n");
        return 1;
    }

    uint32_t addr = (uint32_t) take_int(argv[1]).ok_or(0);
    uint32_t size = (uint32_t) take_int(argv[2]).ok_or(0);

    if (addr == 0 || size == 0) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 1;
    }

    uint32_t checksum = crc32(reinterpret_cast<const uint8_t *>(addr), size);
    printf("CRC32 0x%08lx\r\n", checksum);

    return 0;
}
