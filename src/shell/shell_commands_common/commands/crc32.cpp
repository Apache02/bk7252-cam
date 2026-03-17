#include <stdio.h>
#include <stdint.h>
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include "utils/crc32.h"


#define MAX_SIZE        (1<<20) // 1Mb

int command_crc32(int argc, const char *argv[]) {
    if (argc != 3) {
        printf(COLOR_RED("Usage: crc32 0x<addr> <size>") "\r\n");
        return 1;
    }

    void *addr = take_pointer(argv[1]).ok_or(nullptr);
    int size = take_int(argv[2]).ok_or(-1);

    if (size < 1 || size > MAX_SIZE) {
        printf(COLOR_RED("Invalid size") "\r\n");
        return 1;
    }

    uint32_t checksum = crc32(static_cast<uint8_t *>(addr), static_cast<size_t>(size));
    printf("CRC32 0x%08lx\r\n", checksum);

    return 0;
}
