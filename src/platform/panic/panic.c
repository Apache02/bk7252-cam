#include "platform/panic.h"
#include "utils/busy_wait.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "hardware/gpio.h"
#include "board.h"

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
    bool needWdtRestore = wdt_is_active();
    if (needWdtRestore) wdt_down();

    gpio_config(LED_PIN, GPIO_OUT);
    for (int i = 0; i < count; i++) {
        gpio_put(LED_PIN, 1);
        busy_wait_us(100000);
        gpio_put(LED_PIN, 0);
        busy_wait_us(100000);
    }

    if (needWdtRestore) wdt_up();
#endif // LED_PIN
}

void panic(const char *message) {
    panic_write("PANIC!!!\r\n");
    panic_write(message);
    panic_write("\r\n");

    busy_wait_us(5 * 1000000);
    if (uart1_is_tx_active()) uart1_flush();
    if (uart2_is_tx_active()) uart2_flush();

    wdt_reboot(1000);
}
