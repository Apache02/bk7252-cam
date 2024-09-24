#ifndef _UART_H_
#define _UART_H_

#include "typedef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UART1_PORT                0
#define UART2_PORT                1

typedef void (*uart_callback)(int uport, void *param);

typedef struct uart_callback_des {
    uart_callback callback;
    void *param;
} UART_CALLBACK_RX_T, *UART_CALLBACK_RX_PTR;


void uart_hw_init(UINT8 uport);

int uart_read_byte(int uport);

int uart_write_byte(int uport, char c);

int uart_rx_callback_set(int uport, uart_callback callback, void *param);

int uart_tx_fifo_needwr_callback_set(int uport, uart_callback callback, void *param);

int uart_tx_end_callback_set(int uport, uart_callback callback, void *param);

#ifdef __cplusplus
}
#endif
#endif // _UART_H_
