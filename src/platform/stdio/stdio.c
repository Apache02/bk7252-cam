#if defined(PLATFORM_STDIO_UART1_ENABLED) || defined(PLATFORM_STDIO_UART2_ENABLED)
#include "hardware/uart.h"

#define PLATFORM_STDIO_UART_ENABLED

#ifdef PLATFORM_STDIO_UART1_ENABLED
#define UART_INIT           uart1_init
#define UART_WRITE_BYTE     uart1_write_byte
#define UART_READ_BYTE      uart1_read_byte
#else
#define UART_INIT           uart2_init
#define UART_WRITE_BYTE     uart2_write_byte
#define UART_READ_BYTE      uart2_read_byte
#endif
#endif


void platform_stdio_init() {
#ifdef PLATFORM_STDIO_UART_ENABLED
    UART_INIT();
#endif
}

int _write(int file, char *ptr, int len) {
//    // stdout only
//    if (file != 0) {
//        return -1;
//    }

#ifdef PLATFORM_STDIO_UART_ENABLED
    for (int i = 0; i < len; i++) {
        UART_WRITE_BYTE(*ptr++);
    }
#endif

    return len;
}

int _read(int file, char *ptr, int len) {
    // stdin only
    if (file != 0) {
        return -1;
    }

#ifdef PLATFORM_STDIO_UART_ENABLED
    for (int i = 0; i < len; i++) {
        int c = UART_READ_BYTE();
        if (c == -1) {
            return i == 0 ? -1 : i;
        }
        *ptr++ = c;
    }

    return len;
#else
    return -1;
#endif
}