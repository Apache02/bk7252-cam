#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "platform/stdio.h"
#include "hardware/wdt.h"
#include "hardware/gdma.h"
#include "hardware/time.h"
#include "hardware/sctrl.h"
#include "platform/cpu.h"
#include "soc/gdma.h"
#include "utils/busy_wait.h"


// ============================================================================
// Tally + helpers
// ============================================================================

static unsigned g_total  = 0;
static unsigned g_passed = 0;

static void report(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}

static void fill_pattern(uint8_t *buf, size_t n, uint8_t seed) {
    for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)(seed + i);
}

// Compare buf to the same pattern fill_pattern() would produce.
// On mismatch, reports the first byte that differs.
static bool verify_pattern(const uint8_t *buf, size_t n, uint8_t seed, const char *what) {
    for (size_t i = 0; i < n; i++) {
        uint8_t expected = (uint8_t)(seed + i);
        if (buf[i] != expected) {
            printf("       %s: mismatch at offset %u: got 0x%02x, expected 0x%02x\r\n", what, (unsigned)i, buf[i],
                   expected);
            return false;
        }
    }
    return true;
}

// Quick check that no bytes outside [0, n) were touched.
static bool verify_guard(const uint8_t *buf, size_t guard_size, uint8_t guard_val, const char *what) {
    for (size_t i = 0; i < guard_size; i++) {
        if (buf[i] != guard_val) {
            printf("       %s: guard byte %u corrupted: 0x%02x\r\n", what, (unsigned)i, buf[i]);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Buffers. Sized to comfortably fit large transfers without straining RAM.
// Aligned to 4 to allow 32-bit transfers; tests that need lower alignment
// just offset into them.
// ============================================================================

static constexpr size_t BUF_SIZE = 8192;
static constexpr size_t GUARD    = 16;

static uint8_t *src_buf;
static uint8_t *dst_buf;

// ============================================================================
// Sanity: dump a few key registers so we can see the block is alive.
// ============================================================================

__unused static void dump_regs(const char *when) {
    // direct memory access; same address as in the driver
    auto rd = [](uint32_t addr) { return *(volatile uint32_t *)addr; };
    printf("  regs %s:\r\n", when);
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        printf("    ch%d.config = 0x%08lx\r\n", ch, (unsigned long)rd(0x00809000 + ch * 0x20));
    }
    printf("    int_status = 0x%08lx\r\n", (unsigned long)rd(0x00809000 + 0x38 * 4));
    printf("    prio_mode  = 0x%08lx\r\n", (unsigned long)rd(0x00809000 + 0x37 * 4));
}

// ============================================================================
// Local gdma_memcpy used by the tests.
// Lives in the test file (not the driver) - the driver intentionally exposes
// only the low-level API. Always uses dw=32/32 regardless of alignment: that
// gives BE=1111 on both sides and full bus throughput (4 bytes per dst write).
// Narrower widths are strictly slower for memcpy (see soc/gdma.h transfer
// model notes - they leave 3-byte gaps in dst between values).
// Returns dst on success, NULL on failure. n=0 is a no-op returning dst.
// Max single transfer is 65536 bytes (transfer_length is a 16-bit field, hw stores size-1;
// size is a byte count regardless of dw — see docs/hardware/dma.md).
// ============================================================================
static void *gdma_memcpy(void *dst, const void *src, size_t n) {
    if (dst == nullptr || src == nullptr) {
        return nullptr;
    }
    if (n == 0) {
        return dst;
    }
    // return memcpy(dst, src, n);

    int ch = gdma_reserve_channel();
    if (ch < 0) {
        return nullptr;
    }

    const gdma_config_t cfg = {
        .src =
            {
                .mode          = GDMA_MODE_DTCM,
                .addr          = reinterpret_cast<uint32_t>(src),
                .loop_addr     = 0,
                .loop_end_addr = 0,
                .incr          = true,
                .dw            = GDMA_DATA_WIDTH_32,
            },
        .dst =
            {
                .mode          = GDMA_MODE_DTCM,
                .addr          = reinterpret_cast<uint32_t>(dst),
                .loop_addr     = 0,
                .loop_end_addr = 0,
                .incr          = true,
                .dw            = GDMA_DATA_WIDTH_32,
            },
        .repeat = false,
        .finish   = nullptr,
        .h_finish = nullptr,
    };

    int rc = gdma_run(ch, &cfg, n);
    if (rc != 0) {
        gdma_release_channel(ch);
        return nullptr;
    }

    gdma_wait(ch, 0);
    gdma_release_channel(ch);
    return dst;
}

// ============================================================================
// Correctness tests
// ----------------------------------------------------------------------------
// gdma_memcpy (defined above) always uses dw=32/32 because that gives full
// bus throughput on aligned buffers. OPEN QUESTION: behavior at unaligned
// src/dst with dw=32 has not been verified - hw may mask low address bits,
// crash, or write to unintended bytes. The test_unaligned and
// test_mixed_alignment cases below intentionally probe that; if they fail
// on real hardware, gdma_memcpy needs an alignment-aware fallback to dw=8.
// ============================================================================

__unused static void test_basic_aligned() {
    // both src and dst aligned to 4
    uint8_t         *src = src_buf + GUARD;
    uint8_t         *dst = dst_buf + GUARD;
    constexpr size_t N   = 1024;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x10);

    void *r  = gdma_memcpy(dst, src, N);
    bool  ok = (r == dst) && verify_pattern(dst, N, 0x10, "aligned body") &&
              verify_guard(dst_buf, GUARD, 0xAA, "aligned head guard") &&
              verify_guard(dst_buf + GUARD + N, GUARD, 0xAA, "aligned tail guard");
    report("aligned src/dst, 1024 bytes", ok);
}

__unused static void test_align2() {
    // both aligned to 2 but not 4
    uint8_t         *src = src_buf + GUARD + 2;
    uint8_t         *dst = dst_buf + GUARD + 2;
    constexpr size_t N   = 510;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x20);

    void *r  = gdma_memcpy(dst, src, N);
    bool  ok = (r == dst) && verify_pattern(dst, N, 0x20, "align2 body");
    report("src/dst aligned to 2, 510 bytes", ok);
}

__unused static void test_unaligned() {
    // odd offsets and odd length
    uint8_t         *src = src_buf + GUARD + 1;
    uint8_t         *dst = dst_buf + GUARD + 1;
    constexpr size_t N   = 257;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x30);

    void *r  = gdma_memcpy(dst, src, N);
    bool  ok = (r == dst) && verify_pattern(dst, N, 0x30, "unaligned body");
    report("unaligned src/dst, 257 bytes", ok);
}

__unused static void test_mixed_alignment() {
    // src aligned to 4, dst aligned to 1
    uint8_t         *src = src_buf + GUARD;
    uint8_t         *dst = dst_buf + GUARD + 3;
    constexpr size_t N   = 200;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x40);

    void *r  = gdma_memcpy(dst, src, N);
    bool  ok = (r == dst) && verify_pattern(dst, N, 0x40, "mixed body");
    report("mixed alignment (src@4, dst@1)", ok);
}

__unused static void test_patterns() {
    // Run a few stuck-bit patterns; helps catch a wired-OR or wired-AND bug
    // somewhere in the path.
    static const uint8_t patterns[] = {0x00, 0xFF, 0xAA, 0x55};
    uint8_t             *src        = src_buf + GUARD;
    uint8_t             *dst        = dst_buf + GUARD;
    constexpr size_t     N          = 256;
    bool                 all_ok     = true;

    for (uint8_t p : patterns) {
        memset(src, p, N);
        memset(dst, ~p, N);
        gdma_memcpy(dst, src, N);

        for (size_t i = 0; i < N; i++) {
            if (dst[i] != p) {
                printf("       pattern 0x%02x failed at %u: got 0x%02x\r\n", p, (unsigned)i, dst[i]);
                all_ok = false;
                break;
            }
        }
    }
    report("stuck-bit patterns (0x00/0xFF/0xAA/0x55)", all_ok);
}

__unused static void test_large_transfer() {
    // Largest transfer that fits the buffer; checks that long bursts work.
    uint8_t         *src = src_buf + GUARD;
    uint8_t         *dst = dst_buf + GUARD;
    constexpr size_t N   = BUF_SIZE;

    memset(dst, 0, N);
    fill_pattern(src, N, 0x80);

    void *r  = gdma_memcpy(dst, src, N);
    bool  ok = (r == dst) && verify_pattern(dst, N, 0x80, "large body");
    report("large transfer, 8192 bytes", ok);
}

// ============================================================================
// Edge cases
// ============================================================================

__unused static void test_zero_length() {
    // n == 0 must be a no-op success (returns dst, no side effects).
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;

    memset(dst, 0xCC, 16);
    void *r  = gdma_memcpy(dst, src, 0);
    bool  ok = (r == dst) && (dst[0] == 0xCC) && (dst[15] == 0xCC);
    report("zero-length is no-op", ok);
}

__unused static void test_oversize_rejected() {
    // Max transfer is 65536 bytes (size is a byte count; transfer_length is 16-bit).
    // Pick a size strictly above that to force rejection.
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;

    void *r = gdma_memcpy(dst, src, (size_t)262144 + 4);
    report("oversize transfer rejected (returns NULL)", r == nullptr);
}

__unused static void test_busy_invalid_channel() {
    // Out-of-range channel index must not crash and must report not busy.
    bool ok = !gdma_busy(-1) && !gdma_busy(GDMA_NUM_CHANNELS) && !gdma_busy(99);
    report("gdma_busy on invalid channel", ok);
}

__unused static void test_null_args() {
    // NULL src or dst must be rejected without crashing.
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;
    bool     ok  = (gdma_memcpy(nullptr, src, 16) == nullptr) && (gdma_memcpy(dst, nullptr, 16) == nullptr);
    report("NULL src/dst rejected", ok);
}

// ============================================================================
// Benchmark
// ============================================================================

// Compiler barrier - prevents reordering of measured ops across the line.
#define COMPILER_BARRIER() __asm__ volatile("" ::: "memory")

// Run `iters` copies of `n` bytes from src to dst using GDMA, blocking.
// Returns total elapsed microseconds.
static uint64_t bench_gdma(void *dst, const void *src, size_t n, unsigned iters) {
    COMPILER_BARRIER();
    absolute_time_t t0 = get_absolute_time();
    COMPILER_BARRIER();

    for (unsigned i = 0; i < iters; i++) {
        gdma_memcpy(dst, src, n);
    }

    COMPILER_BARRIER();
    absolute_time_t t1 = get_absolute_time();
    COMPILER_BARRIER();
    return absolute_time_diff_us(t0, t1);
}

// Same shape, but plain libc memcpy.
static uint64_t bench_memcpy(void *dst, const void *src, size_t n, unsigned iters) {
    COMPILER_BARRIER();
    absolute_time_t t0 = get_absolute_time();
    COMPILER_BARRIER();

    for (unsigned i = 0; i < iters; i++) {
        memcpy(dst, src, n);
    }

    COMPILER_BARRIER();
    absolute_time_t t1 = get_absolute_time();
    COMPILER_BARRIER();
    return absolute_time_diff_us(t0, t1);
}

// Format throughput in KB/s. Avoids float printf (the project uses nano-newlib
// without %f by default). Returns integer KB/s; for 100 us / 1 KB ~= 10000 KB/s.
static uint32_t kb_per_sec(size_t bytes_total, uint64_t elapsed_us) {
    if (elapsed_us == 0) return 0;
    // bytes_total / elapsed_us = MB/s, *1000 = KB/s. Compute as
    // (bytes_total * 1000) / elapsed_us, careful with overflow.
    return (uint32_t)((uint64_t)bytes_total * 1000 / elapsed_us);
}

static void bench_one(const char *label, void *dst, const void *src, size_t n, unsigned iters) {
    // warm-up: one call before each measurement to avoid first-iteration cost
    // dominating small-N timings.
    gdma_memcpy(dst, src, n);
    memcpy(dst, src, n);

    uint64_t t_dma = bench_gdma(dst, src, n, iters);
    uint64_t t_cpu = bench_memcpy(dst, src, n, iters);
    size_t   total = n * iters;

    printf("  %-36s n=%5u x%-4u  gdma=%6lu us (%5lu KB/s)  memcpy=%6lu us (%5lu KB/s)\r\n", label, (unsigned)n, iters,
           (unsigned long)t_dma, (unsigned long)kb_per_sec(total, t_dma), (unsigned long)t_cpu,
           (unsigned long)kb_per_sec(total, t_cpu));
}

__unused static void run_benchmark() {
    printf("\r\n-- benchmark (gdma vs libc memcpy) --\r\n");

    uint8_t *src4 = src_buf + GUARD; // aligned to 4
    uint8_t *dst4 = dst_buf + GUARD;
    uint8_t *src2 = src_buf + GUARD + 2; // aligned to 2 only
    uint8_t *dst2 = dst_buf + GUARD + 2;
    uint8_t *src1 = src_buf + GUARD + 1; // aligned to 1
    uint8_t *dst1 = dst_buf + GUARD + 1;

    // make sure src has something defined; content does not matter for timing
    fill_pattern(src_buf, GUARD + BUF_SIZE + GUARD, 0x77);

    // Size sweep at aligned offsets. Both sides aligned to 4, dw=8/8 internal.
    bench_one("size sweep, 64 B", dst4, src4, 64, 1000);
    bench_one("size sweep, 256 B", dst4, src4, 256, 1000);
    bench_one("size sweep, 1 KB", dst4, src4, 1024, 500);
    bench_one("size sweep, 4 KB", dst4, src4, 4096, 100);
    bench_one("size sweep, 8 KB", dst4, src4, 8192, 50);

    // Alignment sweep at fixed size. Driver always uses dw=8/8; this shows
    // whether the +4 address increment misaligned to data boundaries costs
    // anything on real hardware.
    bench_one("align sweep, src/dst @4", dst4, src4, 1024, 500);
    bench_one("align sweep, src/dst @2", dst2, src2, 1024, 500);
    bench_one("align sweep, src/dst @1", dst1, src1, 1023, 500);
}

// ============================================================================
// Loop, repeat, resume, interrupts, arbitration and request lines.
// These use their own word buffers and read channel registers directly where
// the driver has no accessor. Behaviour is documented in docs/hardware/dma.md.
// ============================================================================

static constexpr size_t   WORDS = 512;
static constexpr uint32_t BLANK = 0xEEEEEEEEu;

static uint32_t w_src[WORDS] __attribute__((aligned(4)));
static uint32_t w_dst[WORDS] __attribute__((aligned(4)));

static uint32_t pat(size_t i) { return 0x1000u + (uint32_t)i; }
static uint32_t same(size_t i) { return pat(i); }
static uint32_t blank(size_t) { return BLANK; }

static void load_words() {
    for (size_t i = 0; i < WORDS; i++) {
        w_src[i] = pat(i);
        w_dst[i] = BLANK;
    }
    hw_gdma->int_status.v = hw_gdma->int_status.v;
}

static void wipe(size_t from, size_t count) {
    for (size_t i = from; i < from + count; i++) w_dst[i] = BLANK;
}

template <typename F>
static bool check_words(const char *what, size_t from, size_t count, F expect) {
    for (size_t i = from; i < from + count; i++) {
        const uint32_t want = expect(i);
        if (w_dst[i] != want) {
            printf("       %s: word %u is 0x%08lx, expected 0x%08lx\r\n", what, (unsigned)i, (unsigned long)w_dst[i],
                   (unsigned long)want);
            return false;
        }
    }
    return true;
}

static int highest_written() {
    int hi = -1;
    for (size_t i = 0; i < WORDS; i++) {
        if (w_dst[i] != BLANK) hi = (int)i;
    }
    return hi;
}

static int count_written() {
    int n = 0;
    for (size_t i = 0; i < WORDS; i++) {
        if (w_dst[i] != BLANK) n++;
    }
    return n;
}

static void wait_us(uint32_t us) {
    const absolute_time_t t0 = get_absolute_time();
    while (absolute_time_diff_us(t0, get_absolute_time()) < us) {}
}

// Polls the enable bit; false if the channel was still running after timeout_us.
static bool wait_done(int ch, uint32_t timeout_us) {
    const absolute_time_t t0 = get_absolute_time();
    while (hw_gdma->channels[ch].config.enable) {
        if (absolute_time_diff_us(t0, get_absolute_time()) > timeout_us) return false;
    }
    return true;
}

static uint32_t remain_len(int ch) { return hw_gdma->status[ch].v & 0x1FFFF; }
static unsigned fin_count(int ch) { return (hw_gdma->status[ch].v >> 24) & 0xF; }
static unsigned half_count(int ch) { return (hw_gdma->status[ch].v >> 28) & 0xF; }

// Reserved for the scope; stopped and released on exit so a failing test cannot leave a channel running.
struct Channel {
    int id;
    Channel() : id(gdma_reserve_channel()) {}
    ~Channel() {
        if (id >= 0) gdma_release_channel(id);
    }
    Channel(const Channel &)            = delete;
    Channel &operator=(const Channel &) = delete;
};

struct FixedPriority {
    FixedPriority() { hw_gdma->prio_mode.v = 1; }
    ~FixedPriority() { hw_gdma->prio_mode.v = 0; }
};

struct CpuIrq {
    bool was;
    CpuIrq() : was(portENABLED_IRQ() != 0) { portENABLE_IRQ(); }
    ~CpuIrq() {
        if (!was) portDISABLE_IRQ();
    }
};

static gdma_config_t word_copy() {
    gdma_config_t c = {};
    c.src.mode      = GDMA_MODE_DTCM;
    c.src.addr      = (uint32_t)w_src;
    c.src.incr      = true;
    c.src.dw        = GDMA_DATA_WIDTH_32;
    c.dst.mode      = GDMA_MODE_DTCM;
    c.dst.addr      = (uint32_t)w_dst;
    c.dst.incr      = true;
    c.dst.dw        = GDMA_DATA_WIDTH_32;
    return c;
}

static void src_window(gdma_config_t &c, size_t words) {
    c.src.loop_addr     = (uint32_t)w_src;
    c.src.loop_end_addr = (uint32_t)(w_src + words);
}

static void dst_window(gdma_config_t &c, size_t words) {
    c.dst.loop_addr     = (uint32_t)w_dst;
    c.dst.loop_end_addr = (uint32_t)(w_dst + words);
}

static bool run_to_end(int ch, const gdma_config_t &c, size_t size) {
    return gdma_run(ch, &c, size) == 0 && wait_done(ch, 3000);
}

// ---------------------------------------------------------------------------
// Address loop
// ---------------------------------------------------------------------------

static void test_dst_loop_wraps() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    dst_window(c, 16);
    // 64 words through a 16-word window: the window ends up holding the last 16 words; the end is exclusive.
    const bool ok = ch.id >= 0 && run_to_end(ch.id, c, 256) &&
                    check_words("window", 0, 16, [](size_t i) { return pat(48 + i); }) &&
                    check_words("past the window", 16, 16, blank);
    report("dst loop: 64 words through a 16-word window", ok);
}

static void test_src_loop_repeats_pattern() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    src_window(c, 4);
    const bool ok = ch.id >= 0 && run_to_end(ch.id, c, 64) &&
                    check_words("pattern", 0, 16, [](size_t i) { return pat(i % 4); }) &&
                    check_words("past the transfer", 16, 16, blank);
    report("src loop: 4-word window repeats over 16 words", ok);
}

static void test_both_loops() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    src_window(c, 8);
    dst_window(c, 12);
    // dst word k holds src word (k + 12 * lap) % 8 after the last lap; 48 words = 4 laps of the 12-word window.
    const bool ok = ch.id >= 0 && run_to_end(ch.id, c, 192) &&
                    check_words("both", 0, 12, [](size_t i) { return pat((36 + i) % 8); }) &&
                    check_words("past the window", 12, 8, blank);
    report("src and dst loops together (8-word / 12-word windows)", ok);
}

static void test_loop_start_inside_window() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    c.dst.addr      = (uint32_t)(w_dst + 8);
    dst_window(c, 16);
    // First 8 words fill the upper half, then writing wraps to the window start (not back to the start address).
    const bool ok = ch.id >= 0 && run_to_end(ch.id, c, 128) &&
                    check_words("lower half", 0, 8, [](size_t i) { return pat(24 + i); }) &&
                    check_words("upper half", 8, 8, [](size_t i) { return pat(8 + i); }) &&
                    check_words("past the window", 16, 8, blank);
    report("loop: transfer start inside the window wraps to the window start", ok);
}

static void test_loop_partial_wrap() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    dst_window(c, 16);
    // 24 words through a 16-word window: words 16..23 overwrite window words 0..7.
    const bool ok = ch.id >= 0 && run_to_end(ch.id, c, 96) &&
                    check_words("overwritten", 0, 8, [](size_t i) { return pat(16 + i); }) &&
                    check_words("first lap", 8, 8, [](size_t i) { return pat(i); }) &&
                    check_words("past the window", 16, 8, blank);
    report("dst loop: size not a multiple of the window", ok);
}

static void test_loop_narrow(uint8_t dw, const char *name) {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    c.src.dw        = dw;
    c.dst.dw        = dw;
    dst_window(c, 16);
    // The address still advances +4 per write, so 32 bytes at dw=8 are 32 writes and wrap the 16-word window once.
    // Only the written lanes change.
    bool ok = ch.id >= 0 && run_to_end(ch.id, c, 32);
    if (dw == GDMA_DATA_WIDTH_8) {
        ok = ok && check_words("byte lane", 0, 16, [](size_t i) { return (BLANK & ~0xFFu) | (pat(16 + i) & 0xFFu); });
    } else {
        ok = ok && check_words("half-word lane", 0, 16, [](size_t i) { return (BLANK & ~0xFFFFu) | (pat(i) & 0xFFFFu); });
    }
    ok = ok && check_words("past the window", 16, 8, blank);
    report(name, ok);
}

// ---------------------------------------------------------------------------
// Repeat mode
// ---------------------------------------------------------------------------

static void test_repeat_fixed_addresses() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    c.src.incr      = false;
    c.dst.incr      = false;
    c.repeat        = true;
    bool ok         = ch.id >= 0 && gdma_run(ch.id, &c, 16) == 0;
    wait_us(200);
    const bool still_running = gdma_busy(ch.id);
    // Wipe the destination while the channel runs: only another cycle can fill it again.
    w_dst[0] = BLANK;
    w_src[0] = 0xCAFE0001u;
    wait_us(100);
    const bool refilled = w_dst[0] == 0xCAFE0001u && w_dst[1] == BLANK;
    gdma_stop(ch.id);
    wait_us(20);
    ok = ok && still_running && refilled && !gdma_busy(ch.id);
    report("repeat: restarts by itself and keeps rewriting fixed addresses", ok);
}

static void test_repeat_continues_addresses() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    src_window(c, 16);
    dst_window(c, 16);
    c.repeat = true;
    // 10 words per cycle through 16-word windows: if each cycle restarted at the start addresses, words 10..15
    // would never be written.
    bool ok = ch.id >= 0 && gdma_run(ch.id, &c, 40) == 0;
    wait_us(300);
    const bool running = gdma_busy(ch.id);
    gdma_stop(ch.id);
    wait_us(20);
    ok = ok && running && check_words("window", 0, 16, same) && check_words("past the window", 16, 8, blank);
    report("repeat: addresses continue between cycles", ok);
}

static void test_repeat_stop_clears_enable() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    src_window(c, 16);
    dst_window(c, 16);
    c.repeat = true;
    bool ok  = ch.id >= 0 && gdma_run(ch.id, &c, 64) == 0;
    wait_us(1000);
    const bool running = gdma_busy(ch.id);
    gdma_stop(ch.id);
    const bool stopped = !gdma_busy(ch.id);
    report("repeat: runs until stopped, stop clears enable", ok && running && stopped);
}

// ---------------------------------------------------------------------------
// Stop, resume and restart
// ---------------------------------------------------------------------------

struct Partial {
    bool     ok;
    int      hi; // highest destination word written
    uint32_t remain;
};

// Slow 1 KB copy (interval 15, ~36 us at dw=32), stopped ~10 us in.
static Partial interrupted_copy(int ch, uint8_t dw) {
    load_words();
    gdma_config_t c = word_copy();
    c.src.dw        = dw;
    c.dst.dw        = dw;
    c.src.interval  = 15;
    c.dst.interval  = 15;
    // Narrow widths advance the address +4 per write, so keep a runaway write inside the buffer.
    if (dw != GDMA_DATA_WIDTH_32) dst_window(c, 256);
    Partial p = {false, -1, 0};
    if (gdma_run(ch, &c, 1024) != 0) return p;
    wait_us(10);
    gdma_stop(ch);
    p.remain = remain_len(ch);
    p.hi     = highest_written();
    p.ok     = p.hi > 8 && p.hi < 240;
    return p;
}

static void test_remain_len_is_bytes(uint8_t dw, const char *name) {
    Channel ch;
    bool    ok = ch.id >= 0;
    if (ok) {
        const Partial p     = interrupted_copy(ch.id, dw);
        const uint32_t unit = 1u << dw;
        const uint32_t moved = 1024 - p.remain;
        // Allow a few transactions in flight between the two reads.
        const int64_t diff = (int64_t)moved - (int64_t)unit * (p.hi + 1);
        ok                 = p.ok && p.remain > 0 && p.remain < 1024 && moved % unit == 0 && diff >= -3 * (int64_t)unit &&
             diff <= 3 * (int64_t)unit;
        if (!ok) {
            printf("       remain=%lu hi=%d\r\n", (unsigned long)p.remain, p.hi);
        }
    }
    report(name, ok);
}

// A stopped transfer cannot be resumed: every enable 0 -> 1 starts over from the start addresses.
static void test_stop_enable_restarts() {
    Channel ch;
    bool    ok = ch.id >= 0;
    if (ok) {
        const Partial p = interrupted_copy(ch.id, GDMA_DATA_WIDTH_32);
        ok              = p.ok;
        if (ok) {
            wipe(0, 256);
            hw_gdma->channels[ch.id].config.enable = 1;
            ok = wait_done(ch.id, 3000) && check_words("whole copy", 0, 256, same);
        }
    }
    report("stop + bare enable restarts: the whole transfer is rewritten from the start", ok);
}

static void test_stop_with_new_length() {
    Channel ch;
    bool    ok = ch.id >= 0;
    if (ok) {
        const Partial p = interrupted_copy(ch.id, GDMA_DATA_WIDTH_32);
        ok              = p.ok;
        if (ok) {
            wipe(0, 256);
            ok = gdma_start(ch.id, 16) == 0 && wait_done(ch.id, 1000) && check_words("copied", 0, 4, same) &&
                 check_words("not copied", 4, 252, blank);
        }
    }
    report("stop + new transfer_length: the new length is copied from the start", ok);
}

static void test_stop_with_new_destination() {
    Channel ch;
    bool    ok = ch.id >= 0;
    if (ok) {
        const Partial p = interrupted_copy(ch.id, GDMA_DATA_WIDTH_32);
        ok              = p.ok;
        if (ok) {
            wipe(0, 256);
            ok = gdma_restart(ch.id, (uint32_t)(w_dst + 256), 1024) == 0 && wait_done(ch.id, 3000) &&
                 check_words("old destination", 0, 256, blank) &&
                 check_words("new destination", 256, 256, [](size_t i) { return pat(i - 256); });
        }
    }
    report("stop + new destination address: the whole transfer goes to the new address", ok);
}

static void test_enable_after_finish_restarts() {
    Channel ch;
    load_words();
    gdma_config_t c = word_copy();
    bool          ok = ch.id >= 0 && run_to_end(ch.id, c, 64);
    wipe(0, 32);
    hw_gdma->channels[ch.id].config.enable = 1;
    ok = ok && wait_done(ch.id, 3000) && check_words("second run", 0, 16, same) && check_words("past", 16, 16, blank);
    report("bare enable after a finished transfer starts a new one from the start", ok);
}

// ---------------------------------------------------------------------------
// Interrupts
// ---------------------------------------------------------------------------

static volatile unsigned irq_fin, irq_half;
static volatile uint32_t irq_half_remain;

static void on_fin(int) { irq_fin = irq_fin + 1; }
static void on_half(int ch) {
    irq_half        = irq_half + 1;
    irq_half_remain = remain_len(ch);
}

static void test_irq_single_shot() {
    Channel ch;
    CpuIrq  irq;
    if (ch.id < 0) {
        report("interrupts: one finish and one half-finish per single-shot transfer", false);
        return;
    }
    load_words();
    irq_fin         = 0;
    irq_half        = 0;
    irq_half_remain = 0;
    gdma_config_t c = word_copy();
    c.src.interval  = 15;
    c.dst.interval  = 15;
    c.finish        = on_fin;
    c.h_finish      = on_half;
    const unsigned f0 = fin_count(ch.id), h0 = half_count(ch.id);
    bool           ok = ch.id >= 0 && run_to_end(ch.id, c, 1024);
    wait_us(50);
    const uint32_t flags = (1u << ch.id) | (0x100u << ch.id);
    ok = ok && irq_fin == 1 && irq_half == 1 && ((fin_count(ch.id) - f0) & 0xF) == 1 &&
         ((half_count(ch.id) - h0) & 0xF) == 1 && (hw_gdma->int_status.v & flags) == 0;
    // The handler runs after interrupt latency, so it can only see the transfer at or below the half point.
    ok = ok && irq_half_remain > 0 && irq_half_remain <= 512;
    if (!ok) {
        printf("       fin=%u half=%u half_remain=%lu\r\n", irq_fin, irq_half, (unsigned long)irq_half_remain);
    }
    report("interrupts: one finish and one half-finish per single-shot transfer", ok);
}

// Polls the half-finish flag without taking the interrupt and reads remain_len as soon as the flag shows.
static uint32_t remain_at_half_flag(int ch, size_t size) {
    load_words();
    gdma_config_t c = word_copy();
    c.src.interval  = 15;
    c.dst.interval  = 15;
    c.h_finish      = on_half; // sets half_fin_int_enable; never runs, CPU IRQs are masked here
    if (gdma_run(ch, &c, size) != 0) return 0xFFFFFFFFu;
    uint32_t              remain = 0xFFFFFFFFu;
    const absolute_time_t t0     = get_absolute_time();
    while (absolute_time_diff_us(t0, get_absolute_time()) < 4000) {
        if (hw_gdma->int_status.v & (0x100u << ch)) {
            remain = remain_len(ch);
            break;
        }
    }
    gdma_stop(ch);
    hw_gdma->int_status.v = hw_gdma->int_status.v;
    return remain;
}

static void test_half_flag_at_half_the_bytes() {
    Channel ch;
    bool    ok = ch.id >= 0 && !portENABLED_IRQ();
    if (ok) {
        // Polling latency is a few words, so the flag is seen at or just below size / 2.
        const size_t sizes[] = {1024, 600, 2000};
        for (size_t size : sizes) {
            const uint32_t r = remain_at_half_flag(ch.id, size);
            if (r > size / 2 || r + 96 < size / 2) {
                printf("       size %u: remain %lu at the half flag\r\n", (unsigned)size, (unsigned long)r);
                ok = false;
            }
        }
    }
    report("half-finish flag is raised when half of the bytes are left", ok);
}

static void test_irq_counters_need_enable() {
    Channel ch;
    if (ch.id < 0) {
        report("sticky fin/half counters do not move without the interrupt-enable bits", false);
        return;
    }
    load_words();
    gdma_config_t c = word_copy();
    const unsigned f0 = fin_count(ch.id), h0 = half_count(ch.id);
    const bool     ok = ch.id >= 0 && run_to_end(ch.id, c, 64);
    const bool     counters = fin_count(ch.id) == f0 && half_count(ch.id) == h0;
    report("sticky fin/half counters do not move without the interrupt-enable bits", ok && counters);
}

static void test_irq_repeat_per_cycle() {
    Channel ch;
    CpuIrq  irq;
    if (ch.id < 0) {
        report("repeat: one finish and one half-finish interrupt per cycle", false);
        return;
    }
    load_words();
    irq_fin         = 0;
    irq_half        = 0;
    gdma_config_t c = word_copy();
    c.repeat        = true;
    c.src.interval  = 15;
    c.dst.interval  = 15;
    src_window(c, 256);
    dst_window(c, 256);
    c.finish   = on_fin;
    c.h_finish = on_half;
    const unsigned f0 = fin_count(ch.id);
    bool           ok = ch.id >= 0 && gdma_run(ch.id, &c, 1024) == 0;
    wait_us(2000);
    gdma_stop(ch.id);
    wait_us(100);
    const unsigned fin = irq_fin, half = irq_half;
    // About 55 cycles of 36 us fit in 2 ms.
    ok = ok && fin >= 20 && half + 1 >= fin && half <= fin + 1 && ((fin_count(ch.id) - f0) & 0xF) == (fin & 0xF);
    if (!ok) {
        printf("       fin=%u half=%u\r\n", fin, half);
    }
    report("repeat: one finish and one half-finish interrupt per cycle", ok);
}

// ---------------------------------------------------------------------------
// Arbitration
// ---------------------------------------------------------------------------

static void test_fixed_priority_zero_never_runs() {
    Channel ch;
    bool    ok = ch.id >= 0;
    bool    stuck = false, released_by_rr = false, runs_at_one = false;
    if (ok) {
        load_words();
        FixedPriority fp;
        gdma_config_t c = word_copy();
        gdma_configure(ch.id, &c);
        hw_gdma->channels[ch.id].config.channel_priority = 0;
        gdma_start(ch.id, 1024);
        wait_us(1000);
        stuck = gdma_busy(ch.id) && remain_len(ch.id) == 1024 && count_written() == 0;
        // Round-robin does not release a channel that was stuck at priority 0.
        hw_gdma->prio_mode.v = 0;
        wait_us(200);
        released_by_rr = !gdma_busy(ch.id) || count_written() != 0;
        gdma_stop(ch.id);

        hw_gdma->prio_mode.v = 1;
        load_words();
        gdma_configure(ch.id, &c);
        hw_gdma->channels[ch.id].config.channel_priority = 1;
        gdma_start(ch.id, 1024);
        runs_at_one = wait_done(ch.id, 1000) && check_words("priority 1", 0, 256, same);
    }
    report("fixed priority: priority 0 is never granted, even alone", ok && stuck && !released_by_rr);
    report("fixed priority: priority 1 channel alone runs normally", ok && runs_at_one);
}

// Six channels copy 8 KB each through shared 256-byte windows; returns finish times in us (-1 = never).
static bool six_channels(bool fixed, const unsigned prio[6], int64_t t[6]) {
    bool got[6] = {};
    bool ok     = true;
    for (int i = 0; i < 6; i++) {
        got[i] = gdma_reserve_specific_channel(i) == i;
        ok     = ok && got[i];
        t[i]   = -1;
    }
    if (ok) {
        load_words();
        hw_gdma->prio_mode.v = fixed ? 1 : 0;
        for (int i = 0; i < 6; i++) {
            gdma_config_t c = word_copy();
            src_window(c, 64);
            dst_window(c, 64);
            gdma_configure(i, &c);
            hw_gdma->channels[i].config.channel_priority = prio[i];
        }
        const absolute_time_t t0 = get_absolute_time();
        for (int i = 0; i < 6; i++) gdma_start(i, 8192);
        int left = 6;
        while (left > 0) {
            const int64_t now = absolute_time_diff_us(t0, get_absolute_time());
            for (int i = 0; i < 6; i++) {
                if (t[i] < 0 && !hw_gdma->channels[i].config.enable) {
                    t[i] = now;
                    left--;
                }
            }
            if (now > 5000) break;
        }
        hw_gdma->prio_mode.v = 0;
    }
    for (int i = 0; i < 6; i++) {
        if (got[i]) gdma_release_channel(i);
    }
    for (int i = 0; i < 6; i++) ok = ok && t[i] >= 0;
    return ok;
}

static int64_t max2(int64_t a, int64_t b) { return a > b ? a : b; }
static int64_t min2(int64_t a, int64_t b) { return a < b ? a : b; }

// Groups {a0,a1} must finish strictly before {b0,b1}, which must finish before {c0,c1}.
static bool group_order(const int64_t t[6], int a0, int a1, int b0, int b1, int c0, int c1) {
    return max2(t[a0], t[a1]) < min2(t[b0], t[b1]) && max2(t[b0], t[b1]) < min2(t[c0], t[c1]);
}

static void test_round_robin_is_fair() {
    static const unsigned prio[6] = {3, 0, 2, 1, 0, 3}; // ignored in round-robin
    int64_t               t[6];
    bool                  ok = six_channels(false, prio, t);
    if (ok) {
        int64_t lo = t[0], hi = t[0];
        for (int i = 1; i < 6; i++) {
            lo = min2(lo, t[i]);
            hi = max2(hi, t[i]);
        }
        ok = hi - lo <= 40;
    }
    report("round-robin: six channels finish together, priority ignored", ok);
}

static void test_fixed_priority_order() {
    static const unsigned ramp[6]    = {1, 1, 2, 2, 3, 3};
    static const unsigned reverse[6] = {3, 3, 2, 2, 1, 1};
    static const unsigned equal[6]   = {1, 1, 1, 1, 1, 1};
    int64_t               t[6];

    bool ok = six_channels(true, ramp, t) && group_order(t, 4, 5, 2, 3, 0, 1);
    report("fixed priority: higher value is served first (1 1 2 2 3 3)", ok);

    ok = six_channels(true, reverse, t) && group_order(t, 0, 1, 2, 3, 4, 5);
    report("fixed priority: order flips with the priorities (3 3 2 2 1 1)", ok);

    ok = six_channels(true, equal, t) && group_order(t, 4, 5, 2, 3, 0, 1);
    report("fixed priority: equal priorities, higher channel number first", ok);
}

extern "C" void test_gdma() {
    printf("\r\n== GDMA driver test ==\r\n");

    printf("\r\n-- correctness --\r\n");
    test_basic_aligned();
    test_align2();
    test_unaligned();
    test_mixed_alignment();
    test_patterns();
    test_large_transfer();

    printf("\r\n-- edge cases --\r\n");
    test_zero_length();
    test_oversize_rejected();
    test_busy_invalid_channel();
    test_null_args();

    // A runaway repeat channel would hang the CPU; let the watchdog recover the chip.
    wdt_set(10000);
    wdt_up();

    printf("\r\n-- address loop --\r\n");
    test_dst_loop_wraps();
    test_src_loop_repeats_pattern();
    test_both_loops();
    test_loop_start_inside_window();
    test_loop_partial_wrap();
    test_loop_narrow(GDMA_DATA_WIDTH_8, "dst loop with 8-bit widths wraps, only the byte lane changes");
    test_loop_narrow(GDMA_DATA_WIDTH_16, "dst loop with 16-bit widths wraps, only the half-word lane changes");

    printf("\r\n-- repeat mode --\r\n");
    test_repeat_fixed_addresses();
    test_repeat_continues_addresses();
    test_repeat_stop_clears_enable();

    printf("\r\n-- stop, resume, restart --\r\n");
    test_remain_len_is_bytes(GDMA_DATA_WIDTH_8, "remain_len counts bytes (8-bit widths)");
    test_remain_len_is_bytes(GDMA_DATA_WIDTH_16, "remain_len counts bytes (16-bit widths)");
    test_remain_len_is_bytes(GDMA_DATA_WIDTH_32, "remain_len counts bytes (32-bit widths)");
    test_stop_enable_restarts();
    test_stop_with_new_length();
    test_stop_with_new_destination();
    test_enable_after_finish_restarts();

    printf("\r\n-- interrupts --\r\n");
    test_half_flag_at_half_the_bytes();
    test_irq_single_shot();
    test_irq_counters_need_enable();
    test_irq_repeat_per_cycle();

    printf("\r\n-- arbitration --\r\n");
    test_round_robin_is_fair();
    test_fixed_priority_zero_never_runs();
    test_fixed_priority_order();

    wdt_down();
    run_benchmark();

    printf("\r\n%u / %u passed\r\n\r\n", g_passed, g_total);
}

int main() {
    wdt_down();
    platform_stdio_init();
    busy_wait_ms(20);
    setvbuf(stdout, NULL, _IONBF, 0);

    sctrl_rf_init();

    src_buf = new uint8_t[GUARD + BUF_SIZE + GUARD];
    dst_buf = new uint8_t[GUARD + BUF_SIZE + GUARD];

    wdt_set(10000);

    test_gdma();

    printf("==END==\r\n");
    wdt_reboot(100);
    return 0;
}
