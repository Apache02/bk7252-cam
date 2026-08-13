#include "hardware/uart.h"
#include "soc/uart.h"

#include "hardware/gpio.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "platform/cpu.h"
#include "platform/sched.h"
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

// Mutable half behind a pointer, so the descriptor itself is const and costs no RAM.
struct uart_port {
    volatile hw_uart_t *regs;

    // Volatile through the pointer, so the fields behind it are never cached:
    // uart_set_tx_buffer() swaps them from another context, and a cached copy would let the
    // compiler drop the re-read in uart_write_byte() that pairs a write against a detach.
    volatile struct uart_tx *tx;

    uint32_t irq_source;
};

static struct uart_tx tx1 = {NULL};
static struct uart_tx tx2 = {NULL};

static const struct uart_port port1 = {hw_uart1, &tx1, IRQ_SOURCE_UART1};
static const struct uart_port port2 = {hw_uart2, &tx2, IRQ_SOURCE_UART2};


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

    // rx_need_read / rx_stop_end deliberately left off: commit 01e2875 made them live and
    // reproduced task-stack corruption (known_issues.md, Arch8). _read() polls instead.
    uart->irq_enable.v = 0;
    uart->irq_status.v = uart->irq_status.v;
}

static bool uart_is_tx_active(const struct uart_port *p) { return p->regs->config.tx_enable; }

static int uart_read_byte(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;

    if (!uart->fifo_status.rd_ready) return -1;
    return uart->fifo_data.rx;
}

static int uart_fifo_write_blocked(const struct uart_port *p, uint8_t byte) {
    volatile hw_uart_t *uart = p->regs;

    while (!uart->fifo_status.wr_ready);
    uart->fifo_data.v = byte;

    return 1;
}

static void uart_fifo_wait_empty(const struct uart_port *p) {
    while (!p->regs->fifo_status.tx_empty);
}

// wr_ready lags tx_full by 2-3 writes (soc/uart.h), so a false "no room" just queues.
static bool uart_fifo_try_write(const struct uart_port *p, uint8_t byte) {
    volatile hw_uart_t *uart = p->regs;

    if (!uart->fifo_status.wr_ready) return false;
    uart->fifo_data.v = byte;

    return true;
}

// tx_need_write is a level: clearing the enable bit is the only thing that ends the
// interrupt. Skipping the write when the bit already reads as wanted is most of a burst,
// which is why queueing a byte costs one MMIO read instead of a masked read-modify-write.
static void uart_tx_disarm(const struct uart_port *p) {
    if (!p->regs->irq_enable.tx_need_write) return;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    p->regs->irq_enable.tx_need_write = 0;
    GLOBAL_INT_RESTORE();
}

static void uart_tx_arm(const struct uart_port *p) {
    if (p->regs->irq_enable.tx_need_write) return;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    p->regs->irq_enable.tx_need_write = 1;
    GLOBAL_INT_RESTORE();
}

// Sends the head, not the caller's byte - that is what keeps the stream in order.
static void uart_tx_send_byte_blocked(const struct uart_port *p) {
    GLOBAL_INT_DECLARATION();

    for (;;) {
        // Unmasked: the long wait, and the FIFO drains without needing an interrupt.
        while (!p->regs->fifo_status.wr_ready);

        GLOBAL_INT_DISABLE();

        // Dequeue and write stay atomic against the ISR, or it takes the next byte and
        // gets that one out first. Re-checked because it may have refilled the FIFO.
        const bool can_write = p->regs->fifo_status.wr_ready;
        uint8_t    byte;
        if (can_write && ringbuf_get_byte(p->tx->ring, &byte)) uart_fifo_write_blocked(p, byte);

        GLOBAL_INT_RESTORE();

        if (can_write) return;
    }
}

// One byte per masked window, not the whole queue under one: a full 1 KB queue would blind
// the MAC for ~90 ms at 115200. The emptiness test and the disarm share a section, so a
// concurrent write cannot be left queued with the interrupt already off.
static void uart_tx_flush_queue(const struct uart_port *p) {
    struct ringbuf *ring = p->tx->ring;

    if (!ring || ringbuf_is_empty(ring)) return;

    for (;;) {
        GLOBAL_INT_DECLARATION();
        GLOBAL_INT_DISABLE();
        const bool empty = ringbuf_is_empty(ring);
        if (empty) uart_tx_disarm(p);
        GLOBAL_INT_RESTORE();

        if (empty) return;

        uart_tx_send_byte_blocked(p);
    }
}

// NULL returns the port to synchronous writes; either way the queue goes out first, since
// it is about to stop being reachable.
static void uart_set_tx_buffer(const struct uart_port *p, struct ringbuf *ring) {
    uart_tx_flush_queue(p);

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    uart_tx_disarm(p);
    p->tx->ring = ring;
    GLOBAL_INT_RESTORE();
}

static int uart_write_byte(const struct uart_port *p, char byte) {
    volatile hw_uart_t *uart = p->regs;
    struct ringbuf     *ring = p->tx->ring;

    if (!ring) return uart_fifo_write_blocked(p, (uint8_t)byte);

    // Nothing drains a queue while the core cannot take the interrupt, so a byte left in it
    // may never leave. Flushing first keeps this one behind what is already queued.
    if (!portENABLED_IRQ()) {
        uart_tx_flush_queue(p);
        return uart_fifo_write_blocked(p, (uint8_t)byte);
    }

    // ring_buffer.h promises single-producer safety only, and every printf in the system
    // arrives here. Masked per byte, not per string: 1 KB under one mask is ~90 ms.
    for (;;) {
        GLOBAL_INT_DECLARATION();
        GLOBAL_INT_DISABLE();

        // Re-read inside the section, pairing against uart_set_tx_buffer()'s masked swap:
        // arming for a queue the ISR can no longer see leaves the level with nothing to clear it.
        ring = p->tx->ring;

        if (!ring) {
            GLOBAL_INT_RESTORE();
            return uart_fifo_write_blocked(p, (uint8_t)byte);
        }

        // Bypassing the queue while the FIFO has room is what makes a producer slower than
        // the wire raise no interrupt at all - measured, that is the whole console.
        const bool direct = ringbuf_is_empty(ring) && uart_fifo_try_write(p, (uint8_t)byte);
        if (direct) {
            GLOBAL_INT_RESTORE();
            return 1;
        }

        const bool queued = ringbuf_put_byte(ring, (uint8_t)byte);
        if (queued) {
            uart_tx_arm(p); // idempotent
            GLOBAL_INT_RESTORE();
            return 1;
        }

        GLOBAL_INT_RESTORE();

        // Both buffers full. Yielding unmasked is safe because tx_need_write is a level: a
        // wake-up missed in the gap is followed by another as the FIFO drains - but only
        // while the capacity exceeds one ISR pass, which could otherwise empty and disarm.
        const bool can_wake = ringbuf_capacity(ring) > TX_FIFO_DEPTH && uart->irq_enable.tx_need_write &&
                              intc_irq_source_enabled(p->irq_source);
        if (can_wake)
            sched_yield();
        else
            uart_tx_send_byte_blocked(p);
    }
}

static void uart_drain(const struct uart_port *p) {
    uart_tx_flush_queue(p);
    uart_fifo_wait_empty(p);
}

static void uart_set_baudrate(const struct uart_port *p, unsigned long baudrate) {
    uint32_t baud_div           = DEFAULT_CLK_HZ / baudrate;
    p->regs->config.clk_divider = baud_div;
}

// ack whatever fired (write-1-to-clear). tx_stop_end is a real latched edge event and
// needs this; tx_need_write would not - see uart_tx_disarm().
static void uart_isr(const struct uart_port *p) {
    volatile hw_uart_t *uart = p->regs;
    struct ringbuf     *ring = p->tx->ring;

    typeof(uart->irq_status) status = {.v = uart->irq_status.v};
    uart->irq_status.v              = status.v;

    if (!ring) {
        // Nothing here can end the level, so a stale arm - a write that raced a detach -
        // would re-enter forever and never let the core leave exception context.
        uart_tx_disarm(p);
        return;
    }

    // Batching by tx_fifo_count - exact where wr_ready lags - empties the queue in a handful
    // of interrupts. Leaving on room rather than on bytes keeps the bit set, so it re-fires.
    for (unsigned room = TX_FIFO_DEPTH - uart->fifo_status.tx_fifo_count; room; room--) {
        uint8_t byte;

        if (!ringbuf_get_byte(ring, &byte)) {
            uart_tx_disarm(p);
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

    intc_register_irq_handler(port1.irq_source, uart1_isr);
    intc_enable_irq_source(port1.irq_source);
}

void uart2_init() {
    icu_uart2_power_down();
    gpio_config_function(GPIO_FUNC_UART2);
    icu_uart2_clk(DEFAULT_CLK_SOURCE);
    icu_uart2_power_up();

    uart_reset(&port2);

    intc_register_irq_handler(port2.irq_source, uart2_isr);
    intc_enable_irq_source(port2.irq_source);
}

void uart1_set_tx_buffer(struct ringbuf *ring) { uart_set_tx_buffer(&port1, ring); }

void uart2_set_tx_buffer(struct ringbuf *ring) { uart_set_tx_buffer(&port2, ring); }

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

void uart1_set_baudrate(unsigned long baudrate) { uart_set_baudrate(&port1, baudrate); }

void uart2_set_baudrate(unsigned long baudrate) { uart_set_baudrate(&port2, baudrate); }
