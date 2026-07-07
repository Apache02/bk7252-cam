#include "soc/uart.h"

#include "hardware/gpio.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "platform/sched.h"


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

    hw_write_fields(uart->config,
        .tx_enable = 1,
        .rx_enable = 1,
        .data_length = 3,
        .clk_divider = baud_div,
    );

    hw_write_fields(uart->fifo_config,
        .tx_fifo_threshold = TX_FIFO_THRD,
        .rx_fifo_threshold = RX_FIFO_THRD,
        .rx_stop_detect_time = RX_STOP_DETECT_TIME32,
    );

    uart->flow_config.v   = 0;
    uart->wakeup_config.v = 0;

    hw_write_fields(uart->irq_enable,
        .rx_need_read = 1,
        .rx_stop_end = 1,
    );
}

static int uart_read_byte(volatile hw_uart_t *uart) {
    if (!uart->fifo_status.rd_ready) return -1;
    return uart->fifo_data.rx;
}

static const struct {
    volatile hw_uart_t *uart;
    uint32_t            irq_source;
} UART_IRQ_SOURCE[] = {
    {hw_uart1, IRQ_SOURCE_UART1},
    {hw_uart2, IRQ_SOURCE_UART2},
};

static uint32_t uart_irq_source(volatile hw_uart_t *uart) {
    for (size_t i = 0; i < sizeof(UART_IRQ_SOURCE) / sizeof(UART_IRQ_SOURCE[0]); i++) {
        if (UART_IRQ_SOURCE[i].uart == uart) return UART_IRQ_SOURCE[i].irq_source;
    }
    return 0;
}

// tx_need_write (fires while FIFO has room) storms and crashes the chip even scoped
// narrowly (confirmed on hardware) - do not use it. tx_stop_end fires once when the TX
// FIFO becomes fully empty (edge, not level - also confirmed on hardware), so it's used
// here instead: coarser (waits for a full drain, not just "one byte of room"), but safe.
static int uart_write_byte(volatile hw_uart_t *uart, char byte) {
    if (!uart->fifo_status.wr_ready) {
        bool can_wake = intc_irq_source_enabled(uart_irq_source(uart));

        uart->irq_enable.tx_stop_end = 1;
        while (!uart->fifo_status.wr_ready) {
            if (can_wake) sched_yield();
        }
        uart->irq_enable.tx_stop_end = 0;
    }
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
    uint32_t baud_div        = DEFAULT_CLK_HZ / baudrate;
    uart->config.clk_divider = baud_div;
}

// ack whatever fired (write-1-to-clear). tx_stop_end is a real latched edge event and
// needs this; tx_need_write (not used here - see uart_write_byte) would not.
static void uart_isr(volatile hw_uart_t *uart) {
    typeof(uart->irq_status) status = {.v = uart->irq_status.v};
    uart->irq_status.v              = status.v;
}

static void uart1_isr(void) { uart_isr(hw_uart1); }

static void uart2_isr(void) { uart_isr(hw_uart2); }

void uart1_init() {
    icu_uart1_power_down();
    gpio_config_function(GPIO_FUNC_UART1);

    uart_init(hw_uart1);

    icu_uart1_clk(DEFAULT_CLK_SOURCE);
    icu_uart1_power_up();

    // NOTE: this also makes uart_init()'s pre-existing rx_need_read/rx_stop_end enables
    // live for the first time (previously dormant - IRQ_SOURCE_UART1 was never forwarded
    // to the core before). rx_need_read's storm-safety has not been tested on hardware.
    intc_register_irq_handler(IRQ_SOURCE_UART1, uart1_isr);
    intc_enable_irq_source(IRQ_SOURCE_UART1);
}

void uart2_init() {
    icu_uart2_power_down();
    gpio_config_function(GPIO_FUNC_UART2);

    uart_init(hw_uart2);

    icu_uart2_clk(DEFAULT_CLK_SOURCE);
    icu_uart2_power_up();

    // See uart1_init()'s note: also activates the pre-existing rx_need_read/rx_stop_end
    // enables for the first time.
    intc_register_irq_handler(IRQ_SOURCE_UART2, uart2_isr);
    intc_enable_irq_source(IRQ_SOURCE_UART2);
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
