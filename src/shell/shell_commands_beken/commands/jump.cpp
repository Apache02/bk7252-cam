#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include <stdio.h>
#include <stdint.h>
#include "platform/cpu.h"
#include "hardware/intc.h"
#include "hardware/wdt.h"
#include "hardware/timer.h"
#include "utils/busy_wait.h"


#define APP_ADDR     0x00010000u


static void jump(uint32_t addr, unsigned int delay) {
    // Give UART time to flush output before we kill interrupts
    busy_wait_ms(delay);

    portDisableInt();
    intc_reset();
    timer_reset();

    // Fallback by watchdog after 5 seconds
    wdt_init();
    wdt_set(5000);
    wdt_up();

    // Jump — cast address to a plain void function pointer and call it.
    // The Reset handler at that address is responsible for re-initialising
    // the hardware (stack, .data, .bss, interrupts, etc.)
    reinterpret_cast<void (*)(void)>(addr)();
}

int command_jump(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s 0x<addr>\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));

    if (addr == 0) {
        printf("Invalid address\r\n");
        return 1;
    }

    printf("Jumping to 0x%08lx\r\n", addr);

    jump(addr, 500);

    // Never reached
    return 0;
}

int command_jump_app(__unused int argc, __unused const char *argv[]) {
    printf("go os_addr(0x%x)..........\r\n", APP_ADDR);

    jump(APP_ADDR, 100);

    // Never reached
    return 0;
}
