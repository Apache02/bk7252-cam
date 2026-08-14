#include <stdint.h>
#include <stdio.h>
#include "platform/init.h"
#include "platform/stdio.h"
#include "platform/cpu.h"
#include "platform/arm.h"
#include "hardware/intc.h"
#include "hardware/sctrl.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "shell/commands_beken.h"
#include "shell_handlers.h"
#include "utils/busy_wait.h"
#include "soc/sctrl.h"


extern "C" {
// filled by BACKUP_REGISTERS macro; R10 is omitted (see boot_reset.S)
__used __section(".noinit") struct {
    uint32_t cpsr;
    uint32_t r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r11, r12;
    uint32_t sp;
    uint32_t lr;
} boot_entry_state;
}

// Read before sctrl_init() overwrites it with its own sentinel marker.
static uint32_t g_sw_retention;

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

static const char *cpsr_mode_name(uint32_t cpsr) {
    switch (cpsr & CPU_MODE_MASK) {
        case CPU_MODE_USR:
            return "USR";
        case CPU_MODE_FIQ:
            return "FIQ";
        case CPU_MODE_IRQ:
            return "IRQ";
        case CPU_MODE_SVC:
            return "SVC";
        case CPU_MODE_ABT:
            return "ABT (data abort)";
        case CPU_MODE_UND:
            return "UND (undefined instruction)";
        case CPU_MODE_SYS:
            return "SYS";
        default:
            return "unknown";
    }
}

static const char *reset_cause_name() {
    if (g_sw_retention != 0) return "warm reset (soft jump to 0x0)";
    return "WDT or power-on reset";
}

static void print_reset_cause() {
    uart2_puts("\r\nReset cause: ");
    uart2_puts(reset_cause_name());
    uart2_puts("\r\n");
}

static void print_registers() {
    PRINT_REG("\r\nCPSR:    ", boot_entry_state.cpsr);
    uart2_puts("  mode: ");
    uart2_puts(cpsr_mode_name(boot_entry_state.cpsr));
    PRINT_REG("\r\nR0:      ", boot_entry_state.r0);
    PRINT_REG("\r\nR1:      ", boot_entry_state.r1);
    PRINT_REG("\r\nR2:      ", boot_entry_state.r2);
    PRINT_REG("\r\nR3:      ", boot_entry_state.r3);
    PRINT_REG("\r\nR4:      ", boot_entry_state.r4);
    PRINT_REG("\r\nR5:      ", boot_entry_state.r5);
    PRINT_REG("\r\nR6:      ", boot_entry_state.r6);
    PRINT_REG("\r\nR7:      ", boot_entry_state.r7);
    PRINT_REG("\r\nR8:      ", boot_entry_state.r8);
    PRINT_REG("\r\nR9:      ", boot_entry_state.r9);
    PRINT_REG("\r\nR11:     ", boot_entry_state.r11);
    PRINT_REG("\r\nR12:     ", boot_entry_state.r12);
    PRINT_REG("\r\nR13(SP): ", boot_entry_state.sp);
    PRINT_REG("\r\nR14(LR): ", boot_entry_state.lr);
    uart2_puts("\r\n");
}

static void preinit() {
    g_sw_retention = hw_sctrl->sw_retention.v;

    portDISABLE_IRQ();
    portDISABLE_FIQ();
    intc_reset();

    sctrl_init();
}

PREINIT_AT(preinit, 01);

int main() {
    uart2_init();
    print_reset_cause();
    print_registers();
    uart2_puts("\r\n");
    uart2_drain();

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
