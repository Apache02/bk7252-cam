#include <errno.h>
#include <stdlib.h>

#include "platform/stdio.h"

#undef UART_INIT
#undef UART_WRITE_BYTE
#undef UART_READ_BYTE
#undef UART_SET_TX_BUFFER
#undef UART_DRAIN
#undef UART_PUTS
#undef UART_SET_BAUDRATE

#if defined(PLATFORM_STDIO_UART_ENABLED)

#include "hardware/uart.h"
#include "soc/clock.h"
#include "utils/busy_wait.h"
#include "board.h"

#if BOARD_STDIO_UART == 1

#define UART_INIT          uart1_init
#define UART_WRITE_BYTE    uart1_write_byte
#define UART_READ_BYTE     uart1_read_byte
#define UART_SET_TX_BUFFER uart1_set_tx_buffer
#define UART_DRAIN         uart1_drain
#define UART_PUTS          uart1_puts
#define UART_SET_BAUDRATE  uart1_set_baudrate

#elif BOARD_STDIO_UART == 2

#define UART_INIT          uart2_init
#define UART_WRITE_BYTE    uart2_write_byte
#define UART_READ_BYTE     uart2_read_byte
#define UART_SET_TX_BUFFER uart2_set_tx_buffer
#define UART_DRAIN         uart2_drain
#define UART_PUTS          uart2_puts
#define UART_SET_BAUDRATE  uart2_set_baudrate

#else
#error "BOARD_STDIO_UART in board.h must be 1 or 2"
#endif

#else

#define UART_INIT(...)
#define UART_SET_TX_BUFFER(...)
#define UART_DRAIN(...)
#define UART_PUTS(...)
#define UART_SET_BAUDRATE(...)

#endif

void platform_stdio_init() { UART_INIT(); }

void platform_stdio_set_tx_buffer(struct ringbuf *ring) { UART_SET_TX_BUFFER(ring); }

void platform_stdio_drain() { UART_DRAIN(); }

void platform_stdio_puts(const char *s) { UART_PUTS(s); }

void platform_stdio_set_baudrate(unsigned long baudrate) { UART_SET_BAUDRATE(baudrate); }

#ifdef PLATFORM_STDIO_UART_ENABLED

#define MIN_BAUDRATE (XTAL_CLOCK_HZ / ((1 << 14) - 1))
#define MAX_BAUDRATE (XTAL_CLOCK_HZ / 2)

int command_platform_stdio_speed(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Invalid arguments\r\n");
        return 1;
    }

    char *end;
    const long baudrate = strtol(argv[1], &end, 10);
    if (end == argv[1] || *end != '\0' || baudrate <= MIN_BAUDRATE || baudrate > MAX_BAUDRATE) {
        printf("Invalid baudrate %s\r\n", argv[1]);
        return 2;
    }

    printf("OK %ld\r\n", baudrate);
    busy_wait_ms(100);
    platform_stdio_set_baudrate(baudrate);

    return 0;
}

#else

int command_platform_stdio_speed(__unused int argc, __unused const char *argv[]) {
    printf("No console UART\r\n");
    return 1;
}

#endif

static const platform_stdio_tap_t *s_tap;

bool platform_stdio_set_tap(const platform_stdio_tap_t *tap) {
    if (tap == NULL) {
        s_tap = tap;
        return true;
    }

    if (tap->read && tap->write) {
        s_tap = tap;
        return true;
    }

    return false;
}

int _write(int file, char *ptr, const int len) {
    //    // stdout only
    //    if (file != 0) {
    //        return -1;
    //    }

#ifdef UART_WRITE_BYTE
    const char *data = ptr;
    for (int i = 0; i < len; i++) {
        UART_WRITE_BYTE(*data++);
    }
#endif

    if (s_tap) s_tap->write(ptr, (size_t)len);

    return len;
}

__unused static int read_uart(char *ptr, int len) {
#ifdef UART_READ_BYTE
    for (int i = 0; i < len; i++) {
        const int c = UART_READ_BYTE();
        if (c == -1) {
            return i == 0 ? -1 : i;
        }
        if (ptr) *ptr++ = c;
    }

    return len;
#else
    return -1;
#endif
}

static int read_tap(const platform_stdio_tap_t * tap, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        int c = tap->read();
        if (c == -1) {
            return i == 0 ? -1 : i;
        }
        *ptr++ = c;
    }

    return len;
}

int _read(int file, char *ptr, int len) {
    // stdin only
    if (file != 0) {
        errno = EBADF;
        return -1;
    }

    const platform_stdio_tap_t *tap = s_tap;
    if (tap) {
        const int l = read_tap(tap, ptr, len);
        read_uart(NULL, len);
        return l;
    }

#ifdef UART_READ_BYTE
    return read_uart(ptr, len);
#else
    return -1;
#endif
}
