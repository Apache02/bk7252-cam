#include <stdio.h>
#include "drivers/uart.h"
#include "drivers/wdt.h"

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

void wdt_reboot(void) {
    wdt_set(10);
    wdt_up();
    while (1);
}

int main() {
    uart_hw_init(UART1_PORT);
    uart_hw_init(UART2_PORT);

    for (int i = 0; i < 100; i++) {
        printf("blink %d\r\n", i);

        sleep(1);
        wdt_ping();
    }

    printf("\r\nreboot system\r\n");
    msleep(100);
    wdt_reboot();

    return 0;
}
