#include <stdio.h>
#include "drivers/uart.h"
#include "drivers/gpio.h"
#include "utils/sleep.h"


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
