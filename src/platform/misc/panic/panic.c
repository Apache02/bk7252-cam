#include <stddef.h>

#include "platform/panic.h"
#include "utils/busy_wait.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "hardware/gpio.h"
#include "board.h"

static inline void panic_write(const char *message) {
    if (uart1_is_tx_active()) uart1_puts(message);
    if (uart2_is_tx_active()) uart2_puts(message);
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
    // Detaching flushes what led up to the crash and returns the port to synchronous writes,
    // so the dump below cannot depend on an interrupt the crash may have made impossible.
    //
    // By port, and both ports: a consumer may drive a UART directly with no stdio in the
    // image at all, and a crash dump has to get out either way.
    if (uart1_is_tx_active()) uart1_set_tx_buffer(NULL);
    if (uart2_is_tx_active()) uart2_set_tx_buffer(NULL);

    panic_write("PANIC!!!\r\n");
    panic_write(message);
    panic_write("\r\n");

    busy_wait(1);
    if (uart1_is_tx_active()) uart1_drain();
    if (uart2_is_tx_active()) uart2_drain();

    wdt_reboot(1000);
}
