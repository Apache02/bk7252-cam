#include <stdint.h>
#include <stdio.h>
#include "platform/init.h"
#include "platform/stdio.h"
#include "platform/cpu.h"
#include "hardware/intc.h"
#include "hardware/sctrl.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "shell/commands_beken.h"
#include "shell_handlers.h"
#include "utils/busy_wait.h"


extern "C" {
// filled by BACKUP_REGISTERS macro
__used __section(".noinit") struct {
    uint32_t cpsr;
    uint32_t r[13];
    uint32_t sp;
    uint32_t lr;
} boot_entry_state;
}

#define STEP_MS 100

static bool enter_shell(int seconds) {
    for (int i = seconds; i > 0; i--) {
        printf("\rPress [enter] to stop autoboot: %2d", i);

        for (int j = 0; j < 1000; j += STEP_MS) {
            int c;
            while ((c = getchar()) >= 0) {
                if (c == '\r' || c == '\n') {
                    return true;
                };
            }

            busy_wait_ms(STEP_MS);
        }
    }

    return false;
}

static void print_version() { printf("Bootloader by Apache02\r\n\n"); }

static inline int hex_digit(int digit) { return (digit > 9 ? 'a' - 0xa : '0') + digit; }

void uart2_print_uint32(uint32_t value) {
    char buf[12] = {0};
    int  i       = 0;
    buf[i++]     = '0';
    buf[i++]     = 'x';
    buf[i++]     = hex_digit((value >> (4 * 7)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 6)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 5)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 4)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 3)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 2)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 1)) & 0xf);
    buf[i++]     = hex_digit((value >> (4 * 0)) & 0xf);
    buf[i]       = '\0';
    uart2_puts(buf);
}

#define PRINT_REG(name, value) \
    uart2_puts(name);          \
    uart2_print_uint32(value);

static void print_registers() {
    PRINT_REG("\r\nCPSR:    ", boot_entry_state.cpsr);
    PRINT_REG("\r\nR0:      ", boot_entry_state.r[0]);
    PRINT_REG("\r\nR1:      ", boot_entry_state.r[1]);
    PRINT_REG("\r\nR2:      ", boot_entry_state.r[2]);
    PRINT_REG("\r\nR3:      ", boot_entry_state.r[3]);
    PRINT_REG("\r\nR4:      ", boot_entry_state.r[4]);
    PRINT_REG("\r\nR5:      ", boot_entry_state.r[5]);
    PRINT_REG("\r\nR6:      ", boot_entry_state.r[6]);
    PRINT_REG("\r\nR7:      ", boot_entry_state.r[7]);
    PRINT_REG("\r\nR8:      ", boot_entry_state.r[8]);
    PRINT_REG("\r\nR9:      ", boot_entry_state.r[9]);
    PRINT_REG("\r\nR10:     ", boot_entry_state.r[10]);
    PRINT_REG("\r\nR11:     ", boot_entry_state.r[11]);
    PRINT_REG("\r\nR12:     ", boot_entry_state.r[12]);
    PRINT_REG("\r\nR13(SP): ", boot_entry_state.sp);
    PRINT_REG("\r\nR14(LR): ", boot_entry_state.lr);
    uart2_puts("\r\n");
}

static void preinit() {
    portDISABLE_IRQ();
    portDISABLE_FIQ();
    intc_reset();

    sctrl_init();

    uart2_init();
    print_registers();
    uart2_puts("\r\n");
}

PREINIT_AT(preinit, 01);

int main() {
    wdt_down();
    platform_stdio_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    print_version();

    if (!enter_shell(2)) {
        printf("\r\n");
        const char *args[] = {"go", "app"};
        command_jump(2, args);

        return 0;
    }

    printf("\r\nBootloader shell is ready.\r\n\r\n");

    Shell *console = new Shell(shell_handlers);
    console->reset();
    console->start();
    for (;;) {
        int c = getchar();
        if (c >= 0) {
            console->update(c);
        } else {
            busy_wait_ms(1);
        };
    }

    return 0;
}
