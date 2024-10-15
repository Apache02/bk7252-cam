#include <stdio.h>
#include "platform/stdio.h"
#include "hardware/gpio.h"
#include "hardware/intc.h"
#include "hardware/timer.h"
#include "port/port.h"
#include "utils/sleep.h"
#include "handlers.h"


#define LED_PIN         26
#define KEY_PWR_PIN     2       // pull up
#define KEY_MODE_PIN    7       // pull up


static void init_hardware() {
    intc_init();
    portENABLE_IRQ();
    portENABLE_FIQ();
    timer_init();

    platform_stdio_init();

    gpio_config(LED_PIN, GPIO_OUT);
    gpio_config(KEY_PWR_PIN, GPIO_IN_PULLUP);
    gpio_config(KEY_MODE_PIN, GPIO_IN_PULLUP);
}

static void print_welcome() {
    printf("\r\n%s.\r\n\r\n", COLOR_WHITE("Console is ready"));
}

static bool is_connected() {
    return true;
}

int main() {
    init_hardware();
    // disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    usleep(100'000);

    Console *console = new Console(handlers);

    for (;;) {
        print_welcome();

        console->reset();
        console->start();

        while (is_connected()) {
            char rx[8];
            size_t count = 0;

            for (int c; count < sizeof(rx) && (c = getchar()) != -1;) {
                rx[count++] = c;
            }

            if (count > 0) {
                for (int i = 0; i < count; i++) {
                    int c = rx[i];
                    if (c == '\x1B') {
                        c = console->resolve_key(&rx[i], count);
                        i = count;
                    }
                    console->update(c);
                }
            }

            usleep(1000);
        }
    }

    return 0;
}
