// Interrupt-driven UART TX: what the queue costs and what it buys.
//
// Both regimes run in one image, the queue being attached and detached at runtime. Each
// threshold below carries the value measured on an A9_B_V1_3 at 115200, so drift reads
// differently from breakage.
//
// Time inside _write(), microseconds, for the cases this image actually measures:
//
//     bytes   no queue   queue    wire
//        64         58     114    5504
//       512      32952    1099   44032
//      2048          -   78572  176128
//
// Below the FIFO depth the queue is pure overhead; between the FIFO and the queue capacity
// it removes the wait almost entirely; past the queue the caller waits for the excess
// either way.

#include <stdio.h>
#include <stdint.h>

#include "platform/stdio.h"
#include "platform/cpu.h"

#include "hardware/wdt.h"
#include "hardware/uart.h"
#include "hardware/intc.h"
#include "hardware/time.h"

#include "soc/uart.h"

#include "utils/busy_wait.h"
#include "utils/ring_buffer.h"


extern "C" int _write(int file, char *ptr, int len);


// ============================================================================
// Tally
// ============================================================================

static unsigned g_total  = 0;
static unsigned g_passed = 0;

static void report(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}


// ============================================================================
// Fixtures
// ============================================================================

#define QUEUE_BYTES   (1024u)
#define TX_FIFO_DEPTH (128u) // confirmed on hardware, see soc/uart.h
#define US_PER_BYTE   (10u * 1000000u / 115200u)

RINGBUF_DECLARE(tx, QUEUE_BYTES);

#define MAX_BYTES (2048u)
static char g_block[MAX_BYTES];

static volatile uint32_t g_isr_count;

// Counts only. The driver's handler, registered first by uart2_init(), does the work.
extern "C" void test_count_isr(void) { g_isr_count = g_isr_count + 1; }

static void wait_fifo_empty(void) {
    while (!hw_uart2->fifo_status.tx_empty);
}

// Nothing queued, FIFO empty, counter zeroed - so one case cannot bleed into the next.
static void quiesce(void) {
    platform_stdio_drain();
    wait_fifo_empty();
    g_isr_count = 0;
}

static void fill(unsigned bytes) {
    for (unsigned i = 0; i < bytes; i++) g_block[i] = ((i % 64u) == 63u) ? '\n' : '.';
}

static unsigned wire_us(unsigned bytes) { return bytes * US_PER_BYTE; }

struct sample {
    unsigned write_us; // time inside _write()
    unsigned total_us; // until the bytes are on the wire
    unsigned isrs;
};

// Writes `bytes` and drains synchronously. Measures the caller's cost, not the ISR's -
// the drain deliberately does the remaining work here so the timing is deterministic.
static struct sample measure(unsigned bytes) {
    fill(bytes);
    quiesce();

    const uint32_t t0 = get_us_counter();
    _write(1, g_block, (int)bytes);
    const uint32_t t1 = get_us_counter();

    platform_stdio_drain();
    const uint32_t t2 = get_us_counter();

    struct sample s;
    s.write_us = (unsigned)(t1 - t0);
    s.total_us = (unsigned)(t2 - t0);
    s.isrs     = (unsigned)g_isr_count;

    return s;
}


// ============================================================================
// Results, collected before anything is printed so printf cannot perturb them
// ============================================================================

static struct sample s_small_off, s_small_on;   // 64: fits the FIFO
static struct sample s_mid_off, s_mid_on;       // 512: absorbed by the queue
static struct sample s_big_on;                  // 2048: past the queue
static struct sample s_no_queue_isrs;           // 512 with no queue: must raise nothing

static unsigned s_bg_queued;  // bytes still queued when _write() returned
static unsigned s_bg_isrs;    // interrupts the background drain used
static bool     s_bg_emptied; // did the ISR alone empty the queue

static unsigned s_detach_queued;
static bool     s_detach_emptied;
static unsigned s_detach_tx_bit;


int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20); // UART settle, see F8 in docs/known_issues.md
    setvbuf(stdout, NULL, _IONBF, 0);
    wdt_set(10000);
    wdt_up();

    printf("\r\n=== uart tx queue ===\r\n");

    intc_register_irq_handler(IRQ_SOURCE_UART2, test_count_isr);
    portENABLE_IRQ();

    // ---- cost and benefit, both regimes ------------------------------------
    platform_stdio_set_tx_buffer(NULL);
    s_small_off     = measure(64);
    s_mid_off       = measure(512);
    s_no_queue_isrs = measure(512);

    platform_stdio_set_tx_buffer(&tx);
    s_small_on = measure(64);
    s_mid_on   = measure(512);
    s_big_on   = measure(2048);

    // ---- the ISR must be able to finish the job on its own -----------------
    fill(512);
    quiesce();
    _write(1, g_block, 512);
    s_bg_queued = (unsigned)ringbuf_count(&tx);

    // No drain: only the ISR may empty this. 512 B is ~44 ms on the wire.
    const uint32_t bg_start = get_us_counter();
    while (!ringbuf_is_empty(&tx) && (get_us_counter() - bg_start) < 500000u);

    s_bg_emptied = ringbuf_is_empty(&tx);
    s_bg_isrs    = (unsigned)g_isr_count;
    platform_stdio_drain();

    // ---- detaching must not orphan what is queued --------------------------
    fill(512);
    quiesce();
    _write(1, g_block, 512);
    s_detach_queued = (unsigned)ringbuf_count(&tx);

    platform_stdio_set_tx_buffer(NULL);
    s_detach_emptied = ringbuf_is_empty(&tx);
    s_detach_tx_bit  = hw_uart2->irq_enable.tx_need_write;

    intc_disable_irq_source(IRQ_SOURCE_UART2);

    // ============================================================================
    // Verdicts
    // ============================================================================

    printf("\r\n bytes | no queue: write/total |  queue: write/total/isr | wire\r\n");
    printf("  %4u | %7u %7u       | %7u %7u %4u   | %6u\r\n", 64u, s_small_off.write_us,
           s_small_off.total_us, s_small_on.write_us, s_small_on.total_us, s_small_on.isrs,
           wire_us(64));
    printf("  %4u | %7u %7u       | %7u %7u %4u   | %6u\r\n", 512u, s_mid_off.write_us,
           s_mid_off.total_us, s_mid_on.write_us, s_mid_on.total_us, s_mid_on.isrs, wire_us(512));
    printf("  %4u | %7s %7s       | %7u %7u %4u   | %6u\r\n", 2048u, "-", "-", s_big_on.write_us,
           s_big_on.total_us, s_big_on.isrs, wire_us(2048));
    printf("\r\n");

    // A write inside the FIFO never waits on the wire, with or without a queue.
    // ~58 and ~114 us against a 1376 us bound.
    report("64 B does not wait on the wire, no queue", s_small_off.write_us < wire_us(64) / 4);
    report("64 B does not wait on the wire, queued", s_small_on.write_us < wire_us(64) / 4);

    // Past the FIFO the unbuffered caller pays for every excess byte.
    // ~32952 us against (512-128)*86 = 33024.
    report("512 B without a queue waits for the excess",
           s_mid_off.write_us >= (wire_us(512 - TX_FIFO_DEPTH) * 7u) / 10u);

    // ...and the queue is what removes that wait. ~1099 vs ~32952.
    report("512 B with a queue returns an order faster",
           s_mid_on.write_us * 5u < s_mid_off.write_us);

    // Buffering must not cost throughput: the wire is the bound either way.
    // ~44426 both, against a 55040 us bound.
    report("512 B reaches the wire in the same time, no queue",
           s_mid_off.total_us < (wire_us(512) * 125u) / 100u);
    report("512 B reaches the wire in the same time, queued",
           s_mid_on.total_us < (wire_us(512) * 125u) / 100u);

    // Past FIFO + queue the caller waits for what neither can absorb.
    // ~78572 us against (2048-1024-128)*86 = 77056.
    {
        const unsigned expect = wire_us(2048 - QUEUE_BYTES - TX_FIFO_DEPTH);
        report("2048 B with a queue waits only for the excess",
               s_big_on.write_us > (expect * 7u) / 10u && s_big_on.write_us < (expect * 13u) / 10u);
    }

    // No queue means no TX interrupt at all.
    report("no queue raises no interrupts", s_no_queue_isrs.isrs == 0);

    // Past FIFO + queue the caller sleeps and the ISR drains, one interrupt per FIFO
    // threshold - measured 14 for the ~896 excess bytes. Producer-assist would show none,
    // so this is what distinguishes the two.
    report("2048 B past the queue drains by interrupt", s_big_on.isrs > 0);

    // The ISR alone must finish a queued burst. Measured 6-7 interrupts for 512 B.
    report("bytes really were queued for the background drain", s_bg_queued > 0);
    report("the isr emptied the queue unaided", s_bg_emptied);
    report("the background drain used interrupts", s_bg_isrs > 0);

    // Detaching flushes rather than orphans.
    report("bytes really were queued at detach", s_detach_queued > 0);
    report("detach emptied the queue", s_detach_emptied);
    report("detach disarmed tx_need_write", s_detach_tx_bit == 0);

    printf("\r\n%u / %u passed\r\n", g_passed, g_total);
    printf("==END==\r\n");
    platform_stdio_drain();
    wdt_reboot(100);
    return 0;
}
