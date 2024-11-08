#include "hardware/uart.h"
#include "hardware/wdt.h"
#include "utils/sleep.h"


int uart1_write(const char *message) {
    int count = -1;
    for (const char *tmp = message; *tmp != '\0'; tmp++) {
        count += uart1_write_byte(*tmp);
    }

    return count;
}

int uart2_write(const char *message) {
    int count = -1;
    for (const char *tmp = message; *tmp != '\0'; tmp++) {
        count += uart2_write_byte(*tmp);
    }

    return count;
}

int main() {
    uart1_init();
    uart2_init();

    int c = 0;
    for (int i = 0; i < 10; i++) {
        uart1_write("Hello UART1\r\n");
        uart2_write("Hello UART2\r\n");

        do {
            c = uart1_read_byte();
            if (c >= 0) uart1_write_byte(c);
        } while (c >= 0);

        do {
            c = uart2_read_byte();
            if (c >= 0) uart2_write_byte(c);
        } while (c >= 0);

        msleep(500);
    }

    const char *message = "\r\nreboot system\r\n\r\n";
    uart1_write(message);
    uart2_write(message);

    uart1_flush();
    uart2_flush();

    msleep(200);

    wdt_reboot(100);

    return 0;
}
