#include <stdio.h>
#include "drivers/uart.h"
#include "drivers/gpio.h"


static inline void busy_wait_at_least_cycles(uint32_t minimum_cycles) {
    __asm volatile(
    ".syntax unified\n"
    "1: subs %0, #3\n"
    "bcs 1b\n"
    : "+l" (minimum_cycles) : : "cc", "memory"
    );
}

void usleep(unsigned int us) {
    busy_wait_at_least_cycles(us * (180'000'000 / 1'000'000));
}

void msleep(unsigned int ms) {
    usleep(ms * 1000);
}

void sleep(unsigned int sec) {
    usleep(sec * 1000000);
}

#define LED_PIN         26
#define KEY_PWR_PIN     2       // pull up
#define KEY_MODE_PIN    7       // pull up

int main() {
    uart_hw_init(UART1_PORT);
    uart_hw_init(UART2_PORT);

    gpio_config(LED_PIN, GMODE_OUTPUT);
    gpio_config(KEY_PWR_PIN, GMODE_INPUT_PULLUP);
    gpio_config(KEY_MODE_PIN, GMODE_INPUT_PULLUP);

    struct {
        bool pwr;
        bool mode;
    } keys = {false, false};

    for (;;) {
        bool pressed;

        pressed = gpio_input(KEY_PWR_PIN) == 0;
        if (keys.pwr != pressed) {
            keys.pwr = pressed;
            if (pressed) {
                printf("PWR pressed\r\n");
            } else {
                printf("PWR released\r\n");
            }
        }

        pressed = gpio_input(KEY_MODE_PIN) == 0;
        if (keys.mode != pressed) {
            keys.mode = pressed;
            if (pressed) {
                printf("MODE pressed\r\n");
            } else {
                printf("MODE released\r\n");
            }
        }

        gpio_output(LED_PIN, keys.pwr || keys.mode);

        usleep(100);
    }

    return 0;
}
