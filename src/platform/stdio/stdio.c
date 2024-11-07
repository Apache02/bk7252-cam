#undef UART_INIT
#undef UART_WRITE_BYTE
#undef UART_READ_BYTE

#if defined(PLATFORM_STDIO_UART1_ENABLED)

// uart1
#include "hardware/uart.h"

#define UART_INIT           uart1_init
#define UART_WRITE_BYTE     uart1_write_byte
#define UART_READ_BYTE      uart1_read_byte

#elif defined(PLATFORM_STDIO_UART2_ENABLED)

// uart2
#include "hardware/uart.h"

#define UART_INIT           uart2_init
#define UART_WRITE_BYTE     uart2_write_byte
#define UART_READ_BYTE      uart2_read_byte

#else

#define UART_INIT(...)

#endif


void platform_stdio_init() {
    UART_INIT();
}

int _write(int file, char *ptr, int len) {
//    // stdout only
//    if (file != 0) {
//        return -1;
//    }

#ifdef UART_WRITE_BYTE
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

#ifdef UART_READ_BYTE
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
