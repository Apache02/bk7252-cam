#include "shell/commands_common.h"
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include <stdint.h>
#include <stdio.h>

#define DUMP_DEFAULT_ADDRESS 0x00000000

static bool validate_addr_or_print_error(uint32_t addr) {
    if (addr & 3) {
        printf("%s\r\n", COLOR_RED("addr must be aligned to 4"));
        return false;
    }
    return true;
}

int command_dump(int argc, const char *argv[]) {
    auto         addr  = take_int(argv[1]).ok_or(DUMP_DEFAULT_ADDRESS);
    unsigned int count = 16 * 16;

    printf("addr 0x%08x\r\n", addr);

    if (!validate_addr_or_print_error(static_cast<uint32_t>(addr))) {
        return 1;
    }

    for (size_t i = 0; i < count;) {
        printf("%02x ", *(reinterpret_cast<volatile uint8_t *>(addr + i)));
        i += sizeof(uint8_t);
        if (i % 16 == 0) {
            printf("\r\n");
        }
    }

    return 0;
}

int command_dump32(int argc, const char *argv[]) {
    auto         addr       = take_int(argv[1]).ok_or(DUMP_DEFAULT_ADDRESS);
    unsigned int word_count = 16 * 16 / sizeof(uint32_t); // default: 256-byte dump

    if (argc >= 3) {
        word_count = static_cast<unsigned int>(take_int(argv[2]).ok_or(static_cast<int>(word_count)));
    }

    unsigned int count = word_count * sizeof(uint32_t);

    printf("addr 0x%08x\r\n", addr);

    if (!validate_addr_or_print_error(addr)) {
        return 1;
    }

    for (size_t i = 0; i < count;) {
        printf("%08lx ", *(reinterpret_cast<volatile uint32_t *>(addr + i)));
        i += sizeof(uint32_t);
        if (i % 16 == 0) {
            printf("\r\n");
        }
    }

    if (count % 16 != 0) printf("\r\n");

    return 0;
}
