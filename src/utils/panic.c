#include "utils/panic.h"
#include "utils/sleep.h"
#include "drivers/uart.h"
#include "hardware/wdt.h"
#include <stdbool.h>

static inline void infinite_loop() {
    while (true);
}

static inline void panic_write(const char *message) {
    for (; *message != '\0'; message++) {
        uart_write_byte(UART2_PORT, *message); // TODO: unbind from uart2
    }
}

void panic(const char *message) {
    panic_write("PANIC!!!\r\n");
    panic_write(message);
    panic_write("\r\n");

    sleep(10);
    wdt_set(1000);
    wdt_up();
    infinite_loop();
}
