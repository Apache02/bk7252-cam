#include "shell/commands_iram.h"
#include "shell/Parser.h"
#include "shell/console_colors.h"
#include <stdio.h>
#include <stdint.h>
#include "hardware/cpu.h"
#include "utils/crc32.h"
#include "utils/busy_wait.h"

#define IRAM_START      (0x00900000)
#define IRAM_SIZE       (0x40000)


static bool valid_addr(uint32_t addr) {
    return addr >= IRAM_START && addr < (IRAM_START + IRAM_SIZE);
}

int command_iram_load(int argc, const char *argv[]) {
    if (argc != 3) {
        printf(COLOR_RED("Usage: iram_load 0x<addr> <size>") "\r\n");
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));

    if (!valid_addr(addr) || size == 0) {
        printf(COLOR_RED("Invalid arguments") "\r\n");
        return 1;
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
        printf(COLOR_RED("Usage: iram_jump 0x<addr>") "\r\n");
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));

    if (addr == 0) {
        printf(COLOR_RED("Invalid address") "\r\n");
        return 1;
    }

    printf("Jumping to 0x%08lx\r\n", addr);

    // Give UART time to flush output before we kill interrupts
    busy_wait_ms(100);

    portDISABLE_IRQ();
    portDISABLE_FIQ();

    // Jump — cast address to a plain void function pointer and call it.
    // The Reset handler at that address is responsible for re-initialising
    // the hardware (stack, .data, .bss, interrupts, etc.)
    void (*entry)(void) = reinterpret_cast<void (*)(void)>(addr);
    entry();

    // Never reached
    return 0;
}
