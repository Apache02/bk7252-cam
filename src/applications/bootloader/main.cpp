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


#define STEP_MS             100


static bool enter_shell(int seconds) {
    for (int i = seconds; i > 0; i--) {
        printf("\rPress any key to stop autoboot: %2d", i);

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

static void print_version() {
    printf("Bootloader by Apache02\r\n\n");
}

static void preinit() {
    portDISABLE_IRQ();
    portDISABLE_FIQ();
    intc_reset();

    sctrl_init();

    uart2_init();
    uart2_write_byte('\r');
    uart2_write_byte('\n');
}

PREINIT_AT(preinit, 01);

int main() {
    wdt_down();
    platform_stdio_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    print_version();

    if (!enter_shell(2)) {
        printf("\r\n");
        const char *args[] = {"go_app"};
        command_jump_app(1, args);

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
