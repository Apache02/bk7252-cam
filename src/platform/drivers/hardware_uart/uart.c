#include "uart_regs.h"

#include "hardware/icu.h"
#include "hardware/gpio.h"


#define TX_FIFO_THRD                    (0x40)
#define RX_FIFO_THRD                    (0x10)

#define RX_STOP_DETECT_TIME32           (0)
#define RX_STOP_DETECT_TIME64           (1)
#define RX_STOP_DETECT_TIME128          (2)
#define RX_STOP_DETECT_TIME256          (3)


static void uart_init(volatile hw_uart_t *uart) {
    uint32_t baud_div = 26000000 / 115200;

    uart->config.tx_enable = 1;
    uart->config.rx_enable = 1;
    uart->config.irda = 0;
    uart->config.parity_enable = 0;
    uart->config.parity_odd_mode = 0;
    uart->config.stop_length_2 = 0;
    uart->config.clk_divid = baud_div;

    uart->fifo_config.tx_fifo_threshold = TX_FIFO_THRD;
    uart->fifo_config.rx_fifo_threshold = RX_FIFO_THRD;
    uart->fifo_config.rx_stop_detect_time = RX_STOP_DETECT_TIME32;

    uart->flow_config.v = 0;
    uart->wakeup_config.v = 0;

    uart->irq_enable.rx_need_read = 1;
    uart->irq_enable.rx_stop_end = 1;
}

static int uart_read_byte(volatile hw_uart_t *uart) {
    if (!uart->fifo_status.rd_ready) return -1;
    return uart->fifo_data.rx;
}

static int uart_write_byte(volatile hw_uart_t *uart, char byte) {
    while (!uart->fifo_status.wr_ready) {
        /* nothing */;
    }
    uart->fifo_data.tx = byte;

    return 1;
}

void uart1_init() {
    hw_icu->peri_clk_pwd.uart1 = 0;
    hw_icu->peri_clk_mux.uart1 = PERI_CLK_26M_XTAL;

    gpio_config_function(GPIO_FUNC_UART1);

    uart_init(hw_uart1);
}

void uart2_init() {
    hw_icu->peri_clk_pwd.uart2 = 0;
    hw_icu->peri_clk_mux.uart2 = PERI_CLK_26M_XTAL;

    gpio_config_function(GPIO_FUNC_UART2);

    uart_init(hw_uart2);
}

bool uart1_is_tx_active() {
    return hw_uart1->config.tx_enable;
}

bool uart2_is_tx_active() {
    return hw_uart2->config.tx_enable;
}

int uart1_read_byte() {
    return uart_read_byte(hw_uart1);
}

int uart2_read_byte() {
    return uart_read_byte(hw_uart2);
}

int uart1_write_byte(char c) {
    return uart_write_byte(hw_uart1, c);
}

int uart2_write_byte(char c) {
    return uart_write_byte(hw_uart2, c);
}

void uart1_flush() {
    uint32_t tmp = hw_uart1->config.v;

    while (!hw_uart1->fifo_status.tx_empty);
    hw_uart1->config.rx_enable = 0;
    hw_uart1->config.tx_enable = 0;
    hw_uart1->config.v = tmp;
}

void uart2_flush() {
    uint32_t tmp = hw_uart2->config.v;

    while (!hw_uart2->fifo_status.tx_empty);
    hw_uart2->config.rx_enable = 0;
    hw_uart2->config.tx_enable = 0;
    hw_uart2->config.v = tmp;
}
