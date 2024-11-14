#include "commands.h"
#include <stdio.h>
#include "hardware/gpio.h"
#include "utils/busy_wait.h"


#define LED_PIN         26
#define DELAY           50'000


void dot() {
    putchar('.');
    gpio_put(LED_PIN, 1);
    busy_wait_us(DELAY);
    gpio_put(LED_PIN, 0);
    busy_wait_us(DELAY);
}

void dash() {
    putchar('-');
    gpio_put(LED_PIN, 1);
    busy_wait_us(DELAY * 3);
    gpio_put(LED_PIN, 0);
    busy_wait_us(DELAY);
}

void command_sos(Console &c) {
    gpio_config(LED_PIN, GPIO_OUT);

    for (int i = 0; i < 3; i++) {
        dot();
        dot();
        dot();
        dash();
        dash();
        dash();
        dot();
        dot();
        dot();
        busy_wait_us(DELAY * 2);
        putchar(' ');
    }

    printf("\r\n");
}

void command_gpio_blink(Console &c) {
    int pin_num = c.packet.take_int().ok_or(-1);
    if (pin_num < 0 || pin_num > GPIO_NUM_MAX) {
        printf(COLOR_RED("Incorrect pin number") "\r\n");
        return;
    }
    gpio_num_t gpio = static_cast<gpio_num_t>(pin_num);

    gpio_config(gpio, GPIO_OUT);

    printf("gpio #%d: ", gpio);

    for (int i = 0; i < 3; i++) {
        gpio_put(gpio, 1);
        busy_wait_us(DELAY);
        gpio_put(gpio, 0);
        busy_wait_us(DELAY);
        putchar('.');
    }

    printf("\r\n");
}

