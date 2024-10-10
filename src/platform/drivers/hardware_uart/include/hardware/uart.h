#ifndef _HARDWARE_UART_H
#define _HARDWARE_UART_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif


void uart1_init();

void uart2_init();

bool uart1_is_tx_active();

bool uart2_is_tx_active();

int uart1_read_byte();

int uart2_read_byte();

int uart1_write_byte(char c);

int uart2_write_byte(char c);

void uart1_flush();

void uart2_flush();


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_UART_H
