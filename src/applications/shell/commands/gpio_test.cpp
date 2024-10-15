#include "commands.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "utils/sleep.h"


#define LED_PIN         26
#define DELAY           100'000


void dot() {
    putchar('.');
    gpio_put(LED_PIN, 1);
    usleep(DELAY);
    gpio_put(LED_PIN, 0);
    usleep(DELAY);
}

void dash() {
    putchar('-');
    gpio_put(LED_PIN, 1);
    usleep(DELAY * 3);
    gpio_put(LED_PIN, 0);
    usleep(DELAY);
}

void command_blink(Console &c) {
    gpio_config(LED_PIN, GPIO_OUT);

    for (int i=0; i<3; i++) {
        dot();
        dot();
        dot();
        dash();
        dash();
        dash();
        dot();
        dot();
        dot();
        usleep(DELAY * 2);
        putchar(' ');
    }

    printf("\r\n");
}

