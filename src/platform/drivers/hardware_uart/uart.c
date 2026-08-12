#include "hardware/uart.h"
#include "soc/uart.h"

#include "hardware/gpio.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "platform/cpu.h"
#include "utils/ring_buffer.h"


#define TX_FIFO_THRD (0x40)
#define RX_FIFO_THRD (0x10)

#define RX_STOP_DETECT_TIME32  (0)
#define RX_STOP_DETECT_TIME64  (1)
#define RX_STOP_DETECT_TIME128 (2)
#define RX_STOP_DETECT_TIME256 (3)

#define DEFAULT_CLK_SOURCE PERI_CLK_26M_XTAL
#define DEFAULT_CLK_HZ     (26000000)
#define DEFAULT_BAUDRATE   (115200)

// Confirmed on hardware; see soc/uart.h.
#define TX_FIFO_DEPTH (128)


struct uart_tx {
    struct ringbuf *ring;
};

// The mutable half sits behind a pointer so the descriptor itself can be const and land
// in .rodata; only uart_tx costs RAM.
struct uart_port {
    volatile hw_uart_t *regs;
    struct uart_tx     *tx;
};

static struct uart_tx tx1 = {NULL};
static struct uart_tx tx2 = {NULL};

static const struct uart_port port1 = {hw_uart1, &tx1};
static const struct uart_port port2 = {hw_uart2, &tx2};


static void uart_reset(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;

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

    uart->irq_enable.v = 0;
    uart->irq_status.v = uart->irq_status.v;
}

static bool uart_is_tx_active(const struct uart_port *p) { return p->regs->config.tx_enable; }

static int uart_read_byte(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;

    if (!uart->fifo_status.rd_ready) return -1;
    return uart->fifo_data.rx;
}

static int uart_fifo_write(const struct uart_port *p, uint8_t byte) {
    volatile hw_uart_t *uart = p->regs;

    while (!uart->fifo_status.wr_ready);
    uart->fifo_data.v = byte;

    return 1;
}

// wr_ready lags tx_full by 2-3 writes (soc/uart.h), so this can report no room while a
// little remains. Harmless here - the caller queues instead.
static bool uart_fifo_try_write(const struct uart_port *p, uint8_t byte) {
    volatile hw_uart_t *uart = p->regs;

    if (!uart->fifo_status.wr_ready) return false;
    uart->fifo_data.v = byte;

    return true;
}

// tx_need_write is a level, not an edge: it re-asserts while the FIFO has room, so
// clearing the enable bit is the only thing that ends the interrupt. Confirmed on
// hardware - an ISR that writes nothing is re-entered until this runs.
//
// Unmasked because the only caller outside the ISR already holds the mask; uart_tx_arm()
// below does not, and a bitfield write is a read-modify-write the ISR races.
static void uart_tx_stop(const struct uart_port *p) { p->regs->irq_enable.tx_need_write = 0; }

static void uart_tx_arm(const struct uart_port *p) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    p->regs->irq_enable.tx_need_write = 1;
    GLOBAL_INT_RESTORE();
}

// Caller must already have interrupts masked, or the ISR consumes in parallel and the
// stream reorders.
static void uart_tx_flush_queue(const struct uart_port *p) {
    struct ringbuf *ring = p->tx->ring;

    if (!ring || ringbuf_is_empty(ring)) return;

    for (uint8_t byte; ringbuf_get_byte(ring, &byte);) uart_fifo_write(p, byte);
    uart_tx_stop(p);
}

// Sends the head rather than the byte the caller is holding - that is what keeps the
// stream in order.
static void uart_tx_send_oldest(const struct uart_port *p) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    uint8_t byte;
    if (ringbuf_get_byte(p->tx->ring, &byte)) uart_fifo_write(p, byte);

    GLOBAL_INT_RESTORE();
}

static int uart_write_byte(const struct uart_port *p, char byte) {
    struct ringbuf *ring = p->tx->ring;

    if (!ring) return uart_fifo_write(p, (uint8_t)byte);

    // Nothing will drain a queue while the core cannot take the interrupt, so a byte left
    // in it may never leave. Flushing first keeps this one behind what is already queued.
    if (!portENABLED_IRQ()) {
        uart_tx_flush_queue(p);
        return uart_fifo_write(p, (uint8_t)byte);
    }

    // ring_buffer.h promises single-producer safety only, and every printf in the system
    // arrives here - several tasks plus bk_printf() from the WiFi archives. Masking per
    // byte rather than per string: holding it across a 1 KB write would blind the MAC for
    // ~90 ms at 115200.
    for (;;) {
        GLOBAL_INT_DECLARATION();
        GLOBAL_INT_DISABLE();

        // Bypassing the queue while the FIFO has room is what makes a producer slower
        // than the wire raise no interrupt at all - measured, that is the whole console.
        const bool direct = ringbuf_is_empty(ring) && uart_fifo_try_write(p, (uint8_t)byte);
        const bool queued = direct || ringbuf_put_byte(ring, (uint8_t)byte);

        GLOBAL_INT_RESTORE();

        if (direct) return 1;

        if (queued) {
            uart_tx_arm(p); // idempotent
            return 1;
        }

        uart_tx_send_oldest(p);
    }
}

static void uart_drain(const struct uart_port *p) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    uart_tx_flush_queue(p);
    while (!p->regs->fifo_status.tx_empty);

    GLOBAL_INT_RESTORE();
}

static void uart_flush(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;

    uint32_t tmp = uart->config.v;

    while (!uart->fifo_status.tx_empty);
    uart->config.rx_enable = 0;
    uart->config.tx_enable = 0;
    uart->config.v         = tmp;
}

static void uart_set_baudrate(const struct uart_port *p, unsigned long baudrate) {
    uint32_t baud_div           = DEFAULT_CLK_HZ / baudrate;
    p->regs->config.clk_divider = baud_div;
}

// ack whatever fired (write-1-to-clear). tx_stop_end is a real latched edge event and
// needs this; tx_need_write would not - see uart_tx_stop().
static void uart_isr(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;
    struct ringbuf     *ring = p->tx->ring;

    typeof(uart->irq_status) status = {.v = uart->irq_status.v};
    uart->irq_status.v              = status.v;

    if (!ring) return;

    // Batching by tx_fifo_count, which is exact where wr_ready lags, empties the queue in
    // a handful of interrupts rather than one per byte. Leaving the loop on room rather
    // than on bytes leaves the enable bit set, and the level re-fires.
    for (unsigned room = TX_FIFO_DEPTH - uart->fifo_status.tx_fifo_count; room; room--) {
        uint8_t byte;

        if (!ringbuf_get_byte(ring, &byte)) {
            uart_tx_stop(p);
            return;
        }

        uart->fifo_data.v = byte;
    }
}

static void uart1_isr(void) { uart_isr(&port1); }

static void uart2_isr(void) { uart_isr(&port2); }

void uart1_init() {
    icu_uart1_power_down();
    gpio_config_function(GPIO_FUNC_UART1);
    icu_uart1_clk(DEFAULT_CLK_SOURCE);
    icu_uart1_power_up();

    uart_reset(&port1);

    intc_register_irq_handler(IRQ_SOURCE_UART1, uart1_isr);
    intc_enable_irq_source(IRQ_SOURCE_UART1);
}

void uart2_init() {
    icu_uart2_power_down();
    gpio_config_function(GPIO_FUNC_UART2);
    icu_uart2_clk(DEFAULT_CLK_SOURCE);
    icu_uart2_power_up();

    uart_reset(&port2);

    intc_register_irq_handler(IRQ_SOURCE_UART2, uart2_isr);
    intc_enable_irq_source(IRQ_SOURCE_UART2);
}

// One aligned pointer store, called once before use - nothing for a mask to protect.
void uart1_set_tx_buffer(struct ringbuf *ring) { tx1.ring = ring; }

void uart2_set_tx_buffer(struct ringbuf *ring) { tx2.ring = ring; }

bool uart1_is_tx_active() { return uart_is_tx_active(&port1); }

bool uart2_is_tx_active() { return uart_is_tx_active(&port2); }

int uart1_read_byte() { return uart_read_byte(&port1); }

int uart2_read_byte() { return uart_read_byte(&port2); }

int uart1_write_byte(char c) { return uart_write_byte(&port1, c); }

int uart2_write_byte(char c) { return uart_write_byte(&port2, c); }

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

void uart1_drain() { uart_drain(&port1); }

void uart2_drain() { uart_drain(&port2); }

// Drains too, or the caller reconfigures the port with text still queued.
void uart1_flush() {
    uart_drain(&port1);
    uart_flush(&port1);
}

void uart2_flush() {
    uart_drain(&port2);
    uart_flush(&port2);
}

void uart1_set_baudrate(unsigned long baudrate) { uart_set_baudrate(&port1, baudrate); }

void uart2_set_baudrate(unsigned long baudrate) { uart_set_baudrate(&port2, baudrate); }
