#include <errno.h>

#include "platform/stdio.h"

#undef UART_INIT
#undef UART_WRITE_BYTE
#undef UART_READ_BYTE
#undef UART_SET_TX_BUFFER
#undef UART_DRAIN

#if defined(PLATFORM_STDIO_UART1_ENABLED)

// uart1
#include "hardware/uart.h"

#define UART_INIT          uart1_init
#define UART_WRITE_BYTE    uart1_write_byte
#define UART_READ_BYTE     uart1_read_byte
#define UART_SET_TX_BUFFER uart1_set_tx_buffer
#define UART_DRAIN         uart1_drain

#elif defined(PLATFORM_STDIO_UART2_ENABLED)

// uart2
#include "hardware/uart.h"

#define UART_INIT          uart2_init
#define UART_WRITE_BYTE    uart2_write_byte
#define UART_READ_BYTE     uart2_read_byte
#define UART_SET_TX_BUFFER uart2_set_tx_buffer
#define UART_DRAIN         uart2_drain

#else

#define UART_INIT(...)
#define UART_SET_TX_BUFFER(...)
#define UART_DRAIN(...)

#endif

void platform_stdio_init() { UART_INIT(); }

void platform_stdio_set_tx_buffer(struct ringbuf *ring) { UART_SET_TX_BUFFER(ring); }

void platform_stdio_drain() { UART_DRAIN(); }

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
        errno = EBADF;
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
