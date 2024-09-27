#include <stdio.h>
#include "drivers/uart.h"


#define PRINTF_UART1    0
#define PRINTF_UART2    1

int _write(int file, char *ptr, int len) {
    int i;
    for (i = 0; i < len; i++) {
#if PRINTF_UART1 == 1
        uart_write_byte(UART1_PORT, *ptr);
#endif
#if PRINTF_UART2 == 1
        uart_write_byte(UART2_PORT, *ptr);
#endif
        ptr++;
    }
    return len;
}

