#ifndef _HARDWARE_UART_H
#define _HARDWARE_UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ringbuf;

void uart1_init();

void uart2_init();

// Enables interrupt-driven transmission. Without this a port writes straight to the TX
// FIFO and blocks until it has room. Storage is the caller's (see RINGBUF_DECLARE);
// call once, after uartN_init(). Detaching is not supported.
void uart1_set_tx_buffer(struct ringbuf *ring);

void uart2_set_tx_buffer(struct ringbuf *ring);

bool uart1_is_tx_active();

bool uart2_is_tx_active();

int uart1_read_byte();

int uart2_read_byte();

// Counts bytes accepted, which with a queue attached means queued, not transmitted.
int uart1_write_byte(char c);

int uart2_write_byte(char c);

int uart1_puts(const char *s);

int uart2_puts(const char *s);

// Queued output only reaches the wire once the ISR runs, so a path that ends execution -
// a panic dump, a reboot - loses it unless it drains first.
void uart1_drain();

void uart2_drain();

void uart1_flush();

void uart2_flush();

void uart1_set_baudrate(unsigned long baudrate);

void uart2_set_baudrate(unsigned long baudrate);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_UART_H
