#include "shell/commands_iram.h"
#include "shell/Parser.h"
#include "shell/console_colors.h"
#include <stdio.h>
#include <stdint.h>
#include "platform/cpu.h"
#include "hardware/intc.h"
#include "hardware/wdt.h"
#include "utils/crc32.h"
#include "utils/busy_wait.h"

#define IRAM_START      (0x00900000)
#define IRAM_SIZE       (0x40000)


static bool valid_addr(uint32_t addr) {
    return addr >= IRAM_START && addr < (IRAM_START + IRAM_SIZE);
}

static bool inline valid_checksum(uint32_t addr, uint32_t size, uint32_t checksum) {
    return addr + size == checksum;
}

int command_iram_load(int argc, const char *argv[]) {
    if (argc != 4) {
        printf(COLOR_RED("Usage: %s 0x<addr> <size> <addr+size>") "\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
    uint32_t check = static_cast<uint32_t>(take_int(argv[3]).ok_or(0));

    if (!valid_addr(addr)) {
        printf(COLOR_RED("Invalid address") "\r\n");
        return 2;
    }

    if (size == 0) {
        printf(COLOR_RED("Invalid size") "\r\n");
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


int command_iram_jump(int argc, const char *argv[]) {
    if (argc != 2) {
        printf(COLOR_RED("Usage: %s 0x<addr>") "\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));

    if (addr == 0) {
        printf(COLOR_RED("Invalid address") "\r\n");
        return 1;
    }

    printf("Jumping to 0x%08lx\r\n", addr);

    // Give UART time to flush output before we kill interrupts
    busy_wait_ms(500);

    portDisableInt();
    intc_reset();

    // Fallback by watchdog after 5 seconds
    wdt_init();
    wdt_set(5000);
    wdt_up();

    // Jump — cast address to a plain void function pointer and call it.
    // The Reset handler at that address is responsible for re-initialising
    // the hardware (stack, .data, .bss, interrupts, etc.)
    void (*entry)(void) = reinterpret_cast<void (*)(void)>(addr);
    entry();

    // Never reached
    return 0;
}
