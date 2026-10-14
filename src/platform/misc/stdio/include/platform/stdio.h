#ifndef _PLATFORM_STDIO_H
#define _PLATFORM_STDIO_H

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ringbuf;

void platform_stdio_init();

// Route stdout through a TX queue on whichever UART this build is bound to, so callers
// need not name the port. NULL returns it to synchronous writes; no-op with no UART.
void platform_stdio_set_tx_buffer(struct ringbuf *ring);

// Queued output only reaches the wire once the ISR runs, so a path that ends execution
// loses it unless it drains first.
void platform_stdio_drain();

// A second console next to the UART. Everything written to stdout goes to the UART and
// also to write(). Input comes from read() alone while the tap is set; what the UART
// receives meanwhile is discarded. read() returns -1 when it has nothing.
typedef struct {
    void (*write)(const void *data, size_t len);
    int (*read)(void);
} platform_stdio_tap_t;

// The tap has to outlive its use, and both of its functions are required: a tap with
// either one NULL is refused and false is returned. NULL removes the tap.
bool platform_stdio_set_tap(const platform_stdio_tap_t *tap);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_STDIO_H
