#ifndef _PLATFORM_STDIO_H
#define _PLATFORM_STDIO_H

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

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_STDIO_H