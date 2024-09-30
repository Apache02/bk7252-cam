#include <stdio.h>
#include "drivers/uart.h"
#include "drivers/gpio.h"
#include "utils/sleep.h"
#include "utils/console_colors.h"
#include "console/Console.h"


#define LED_PIN         26
#define KEY_PWR_PIN     2       // pull up
#define KEY_MODE_PIN    7       // pull up


void init_hardware() {
    uart_hw_init(UART2_PORT);

    gpio_config(LED_PIN, GMODE_OUTPUT);
    gpio_config(KEY_PWR_PIN, GMODE_INPUT_PULLUP);
    gpio_config(KEY_MODE_PIN, GMODE_INPUT_PULLUP);
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

    Console *console = new Console();

    for (;;) {
        print_welcome();

        console->reset();
        console->start();

        while (is_connected()) {
            char rx[6];
            size_t count = 0;

            for (int c; count < sizeof(rx) && (c = getchar()) != -1;) {
                rx[count++] = c;
            }

            console->update(rx, count);

            usleep(1000);
        }
    }

    return 0;
}
