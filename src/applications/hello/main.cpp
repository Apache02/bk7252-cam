#include <stdio.h>
#include "drivers/uart.h"
#include "drivers/wdt.h"
#include "utils/sleep.h"


void wdt_reboot(void) {
    wdt_set(10);
    wdt_up();
    while (1);
}

int main() {
    uart_hw_init(UART1_PORT);
    uart_hw_init(UART2_PORT);

    for (int i = 0; i < 100; i++) {
        printf("hello %d\r\n", i);

        sleep(1);
        wdt_ping();
    }

    printf("\r\nreboot system\r\n");
    msleep(100);
    wdt_reboot();

    return 0;
}
