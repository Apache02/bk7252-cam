#include <stdio.h>
#include "platform/stdio.h"
#include "hardware/gpio.h"
#include "hardware/intc.h"
#include "hardware/timer.h"
#include "hardware/wdt.h"
#include "utils/busy_wait.h"
#include "shell_handlers.h"


#define LED_PIN         26
#define KEY_PWR_PIN     2       // pull up
#define KEY_MODE_PIN    7       // pull up


static void init_hardware() {
    intc_init();
    timer_init();

    platform_stdio_init();

    gpio_config(LED_PIN, GPIO_OUT);
    gpio_config(KEY_PWR_PIN, GPIO_IN_PULLUP);
    gpio_config(KEY_MODE_PIN, GPIO_IN_PULLUP);
}

static void print_welcome() {
    printf("\r\nRAM loader shell is ready.\r\n\r\n");
}

static bool is_connected() {
    return true;
}

int main() {
    wdt_down();
    init_hardware();
    // disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    busy_wait_us(100'000);

    Shell *console = new Shell(shell_handlers);

    for (;;) {
        print_welcome();

        console->reset();
        console->start();

        while (is_connected()) {
            int c = getchar();
            if (c < 0) {
                busy_wait_us(1000);
            } else {
                console->update(c);
            }
        }
    }

    return 0;
}
