#include <stdio.h>
#include "drivers/uart.h"
//#include "drivers/gpio.h"
#include "utils/sleep.h"



//#define LED_PIN         26
//#define KEY_PWR_PIN     2       // pull up
//#define KEY_MODE_PIN    7       // pull up


int main() {
//    uart_hw_init(UART1_PORT);
    uart_hw_init(UART2_PORT);

//    gpio_config(LED_PIN, GMODE_OUTPUT);
//    gpio_config(KEY_PWR_PIN, GMODE_INPUT_PULLUP);
//    gpio_config(KEY_MODE_PIN, GMODE_INPUT_PULLUP);

    printf("%s begin\r\n", __PRETTY_FUNCTION__);

    for (unsigned int i = 0;; i++) {
        bool flush_required = false;
        int c;
        while ((c = getchar()) != -1) {
            putchar(c);
            flush_required = true;
        }
        if (flush_required) {
            fflush(stdout);
        }

        printf("%d\r\n", i);

        usleep(500'000);
    }

    return 0;
}
