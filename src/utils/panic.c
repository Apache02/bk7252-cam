#include "utils/panic.h"
#include "utils/sleep.h"
#include "hardware/uart.h"
#include "hardware/wdt.h"


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

void panic(const char *message) {
    panic_write("PANIC!!!\r\n");
    panic_write(message);
    panic_write("\r\n");

    sleep(5);
    if (uart1_is_tx_active()) uart1_flush();
    else if (uart2_is_tx_active()) uart2_flush();

    wdt_reboot(1000);
}
