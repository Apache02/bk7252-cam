#include "shell/commands_iram.h"
#include "shell/Parser.h"
#include <stdio.h>
#include <stdint.h>
#include "platform/cpu.h"
#include "hardware/intc.h"
#include "hardware/wdt.h"
#include "utils/crc32.h"
#include "utils/busy_wait.h"

static bool inline valid_ram(uint32_t addr) {
    return (addr >= 0x00400000 && addr < 0x00400000 + 0x00040000) ||
           (addr >= 0x00900000 && addr < 0x00900000 + 0x00040000);
}

static bool inline valid_checksum(uint32_t addr, uint32_t size, uint32_t checksum) { return addr + size == checksum; }

int command_iram_load(int argc, const char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s 0x<addr> <size> <addr+size>\r\n", argv[0]);
        return 1;
    }

    uint32_t addr  = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size  = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
    uint32_t check = static_cast<uint32_t>(take_int(argv[3]).ok_or(0));

    if (!valid_ram(addr)) {
        printf("Invalid address\r\n");
        return 2;
    }

    if (size == 0) {
        printf("Invalid size\r\n");
        return 2;
    }

    if (!valid_checksum(addr, size, check)) {
        printf("Invalid checksum\r\n");
        return 2;
    }

    uint8_t *dest = reinterpret_cast<uint8_t *>(addr);

    printf("READY\r\n");

    // Read exactly <size> raw bytes from serial
    for (uint32_t i = 0; i < size; i++) {
        int c;
        do {
            c = getchar();
        } while (c < 0);
        dest[i] = static_cast<uint8_t>(c & 0xFF);
    }

    uint32_t checksum = crc32(dest, size);
    printf("DONE %08lx\r\n", checksum);

    return 0;
}
