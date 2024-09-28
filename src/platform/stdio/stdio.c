#include "drivers/uart.h"


#define STDIO_UART_PORT     UART2_PORT


int _write(int file, char *ptr, int len) {
//    // stdout only
//    if (file != 0) {
//        return -1;
//    }

    for (int i = 0; i < len; i++) {
        uart_write_byte(STDIO_UART_PORT, *ptr);
        ptr++;
    }
    return len;
}

int _read(int file, char *ptr, int len) {
    // stdin only
    if (file != 0) {
        return -1;
    }

    for (int i = 0; i < len; i++) {
        int c = uart_read_byte(STDIO_UART_PORT);
        if (c == -1) {
            return i == 0 ? -1 : i;
        }
        *ptr++ = c;
    }
    return len;
}