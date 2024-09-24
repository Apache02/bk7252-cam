#include <stdint.h>
#include "drivers/uart.h"
#include "drivers/wdt.h"

void kprint(const char *message) {
    while (*message != '\0') {
        uart_write_byte(UART2_PORT, *message++);
    }
}

void kprint_uart(int uport, const char *message) {
    while (*message != '\0') {
        uart_write_byte(uport, *message++);
    }
}

static inline void busy_wait_at_least_cycles(uint32_t minimum_cycles) {
    __asm volatile(
    ".syntax unified\n"
    "1: subs %0, #3\n"
    "bcs 1b\n"
    : "+l" (minimum_cycles) : : "cc", "memory"
    );
}

void usleep(unsigned int us) {
    busy_wait_at_least_cycles(us * (60'000'000 / 1'000'000));
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

    for (int i = 0; i < 10; i++) {
        kprint_uart(UART1_PORT, "Hello UART1\r\n");
        kprint_uart(UART2_PORT, "Hello UART2\r\n");

        sleep(1);
    }

    kprint("\r\nreboot system\r\n");
    msleep(100);
    wdt_reboot();

    return 0;
}
