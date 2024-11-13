#include "utils/panic.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "hardware/gpio.h"
#include "board.h"

static inline void busy_wait_at_least_cycles(unsigned long minimum_cycles) {
    __asm volatile(
            ".syntax unified\n"
            "1: subs %0, #3\n"
            "bcs 1b\n"
            : "+l" (minimum_cycles) : : "cc", "memory"
            );
}

static void busy_wait(unsigned int us) {
    busy_wait_at_least_cycles(us * (120000000 / 1000000));
}

static inline void panic_write(const char *message) {
    if (uart1_is_tx_active()) {
        for (const char *tmp = message; *tmp != '\0'; tmp++) {
            uart1_write_byte(*tmp);
        }
    } else if (uart2_is_tx_active()) {
        for (const char *tmp = message; *tmp != '\0'; tmp++) {
            uart2_write_byte(*tmp);
        }
    }
}

void panic_blink(int count) {
#ifdef LED_PIN
    gpio_config(LED_PIN, GPIO_OUT);
    for (int i=0; i<count; i++) {
        gpio_put(LED_PIN, 1);
        busy_wait(50000);
        gpio_put(LED_PIN, 0);
        busy_wait(50000);
    }
#endif // LED_PIN
}

void panic(const char *message) {
    panic_write("PANIC!!!\r\n");
    panic_write(message);
    panic_write("\r\n");

    busy_wait(5 * 1000000);
    if (uart1_is_tx_active()) uart1_flush();
    else if (uart2_is_tx_active()) uart2_flush();

    wdt_reboot(1000);
}
