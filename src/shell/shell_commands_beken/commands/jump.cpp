#include <cstring>

#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include <stdio.h>
#include <stdint.h>
#include "platform/cpu.h"
#include "hardware/intc.h"
#include "hardware/wdt.h"
#include "hardware/timer.h"
#include "utils/busy_wait.h"

#define BOOTLOADER_ADDR 0x00000000ul
#define APP_ADDR        0x00010000ul
#define IRAM_ADDR       0x00900000ul

static void jump(uint32_t addr, unsigned int delay, bool disarm_wdt) {
    // Give UART time to flush output before we kill interrupts
    busy_wait_ms(delay);

    portDisableInt();
    intc_reset();
    timer_reset();

    if (disarm_wdt) {
        // Target does not manage the watchdog itself (e.g. firmware built
        // from the original SDK) — leave it disarmed instead of arming a
        // fallback, or it will reset the target out from under it.
        wdt_init();
        wdt_down();
    } else {
        // Fallback by watchdog after 5 seconds
        wdt_init();
        wdt_set(5000);
        wdt_up();
    }

    // Jump — cast address to a plain void function pointer and call it.
    // The Reset handler at that address is responsible for re-initialising
    // the hardware (stack, .data, .bss, interrupts, etc.)
    reinterpret_cast<void (*)(void)>(addr)();
}

int command_jump(int argc, const char *argv[]) {
    if (argc != 2 && argc != 3) {
        printf("Usage: %s 0x<addr> [--disarm_wdt]\r\n", argv[0]);
        return 1;
    }

    bool disarm_wdt = false;
    if (argc == 3) {
        if (strcmp(argv[2], "--disarm_wdt") != 0) {
            printf("Usage: %s 0x<addr> [--disarm_wdt]\r\n", argv[0]);
            return 1;
        }
        disarm_wdt = true;
    }

    if (strcmp(argv[1], "app") == 0) {
        printf("go os_addr(0x%lx)..........\r\n", APP_ADDR);
        jump(APP_ADDR, 100, disarm_wdt);

        return 0;
    }

    if (strcmp(argv[1], "bootloader") == 0) {
        printf("Jumping to bootloader\r\n");
        jump(BOOTLOADER_ADDR, 500, disarm_wdt);

        return 0;
    }

    if (strcmp(argv[1], "iram") == 0) {
        printf("Jumping to iram\r\n");
        jump(IRAM_ADDR, 500, disarm_wdt);

        return 0;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));

    if (addr == 0) {
        printf("Invalid address\r\n");
        return 1;
    }

    printf("Jumping to 0x%08lx\r\n", addr);

    jump(addr, 500, disarm_wdt);

    // Never reached
    return 0;
}
