#include "soc/uart.h"

#include "hardware/icu.h"
#include "hardware/gpio.h"


#define TX_FIFO_THRD (0x40)
#define RX_FIFO_THRD (0x10)

#define RX_STOP_DETECT_TIME32  (0)
#define RX_STOP_DETECT_TIME64  (1)
#define RX_STOP_DETECT_TIME128 (2)
#define RX_STOP_DETECT_TIME256 (3)

#define DEFAULT_CLK_SOURCE PERI_CLK_26M_XTAL
#define DEFAULT_CLK_HZ     (26000000)
#define DEFAULT_BAUDRATE   (115200)

static void uart_init(volatile hw_uart_t *uart) {
    const uint32_t baud_div = DEFAULT_CLK_HZ / DEFAULT_BAUDRATE;

    hw_write_fields(uart->config, .tx_enable = 1, .rx_enable = 1, .data_length = 3, .clk_divid = baud_div, );

    hw_write_fields(uart->fifo_config, .tx_fifo_threshold = TX_FIFO_THRD, .rx_fifo_threshold = RX_FIFO_THRD,
                    .rx_stop_detect_time = RX_STOP_DETECT_TIME32, );

    uart->flow_config.v   = 0;
    uart->wakeup_config.v = 0;

    hw_write_fields(uart->irq_enable, .rx_need_read = 1, .rx_stop_end = 1, );
}

static int uart_read_byte(volatile hw_uart_t *uart) {
    if (!uart->fifo_status.rd_ready) return -1;
    return uart->fifo_data.rx;
}

static int uart_write_byte(volatile hw_uart_t *uart, char byte) {
    while (!uart->fifo_status.wr_ready);
    // uart->fifo_data.tx = byte; is unsafe: compiler may emit read-modify-write
    // for bitfield write, which reads the whole register first.
    // Use .v to guarantee a single 32-bit store.
    uart->fifo_data.v = (uint32_t)(unsigned char)byte;

    return 1;
}

static void uart_flush(volatile hw_uart_t *uart) {
    uint32_t tmp = uart->config.v;

    while (!uart->fifo_status.tx_empty);
    uart->config.rx_enable = 0;
    uart->config.tx_enable = 0;
    uart->config.v         = tmp;
}

static void uart_set_baudrate(volatile hw_uart_t *uart, unsigned long baudrate) {
    uint32_t baud_div      = DEFAULT_CLK_HZ / baudrate;
    uart->config.clk_divid = baud_div;
}

void uart1_init() {
    hw_icu->peri_clk_pwd.uart1 = 1;
    gpio_config_function(GPIO_FUNC_UART1);

    uart_init(hw_uart1);

    icu_uart1_clk(DEFAULT_CLK_SOURCE);
    icu_uart1_power_up();
}

void uart2_init() {
    hw_icu->peri_clk_pwd.uart2 = 1;
    gpio_config_function(GPIO_FUNC_UART2);

    uart_init(hw_uart2);

    icu_uart2_clk(DEFAULT_CLK_SOURCE);
    icu_uart2_power_up();
}

bool uart1_is_tx_active() { return hw_uart1->config.tx_enable; }

bool uart2_is_tx_active() { return hw_uart2->config.tx_enable; }

int uart1_read_byte() { return uart_read_byte(hw_uart1); }

int uart2_read_byte() { return uart_read_byte(hw_uart2); }

int uart1_write_byte(char c) { return uart_write_byte(hw_uart1, c); }

int uart2_write_byte(char c) { return uart_write_byte(hw_uart2, c); }

int uart1_puts(const char *s) {
    int i = 0;
    while (*s) i += uart1_write_byte(*s++);
    return i;
}

int uart2_puts(const char *s) {
    int i = 0;
    while (*s) i += uart2_write_byte(*s++);
    return i;
}

void uart1_flush() { uart_flush(hw_uart1); }

void uart2_flush() { uart_flush(hw_uart2); }

void uart1_set_baudrate(unsigned long baudrate) { uart_set_baudrate(hw_uart1, baudrate); }

void uart2_set_baudrate(unsigned long baudrate) { uart_set_baudrate(hw_uart2, baudrate); }
