#include <stdio.h>
#include "hardware/gpio.h"
#include "platform/stdio.h"
#include "utils/busy_wait.h"


#define LED_PIN         26
#define KEY_PWR_PIN     2       // pull up
#define KEY_MODE_PIN    7       // pull up


int main() {
    platform_stdio_init();

    gpio_config(LED_PIN, GPIO_OUT);
    gpio_config(KEY_PWR_PIN, GPIO_IN);
    gpio_config(KEY_MODE_PIN, GPIO_IN);

    struct {
        bool pwr;
        bool mode;
    } keys = {false, false};

    for (;;) {
        bool pressed;

        pressed = gpio_get(KEY_PWR_PIN) == 0;
        if (keys.pwr != pressed) {
            keys.pwr = pressed;
            if (pressed) {
                printf("PWR pressed\r\n");
                gpio_put(LED_PIN, 1);
            } else {
                printf("PWR released\r\n");
                gpio_put(LED_PIN, 0);
            }
        }

        pressed = gpio_get(KEY_MODE_PIN) == 0;
        if (keys.mode != pressed) {
            keys.mode = pressed;
            if (pressed) {
                printf("MODE pressed\r\n");
                gpio_toggle(LED_PIN);
            } else {
                printf("MODE released\r\n");
            }
        }

        busy_wait_us(100);
    }

    return 0;
}
