#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "platform/stdio.h"
#include "hardware/wdt.h"
#include "hardware/gdma.h"
#include "hardware/time.h"


// ============================================================================
// Tally + helpers
// ============================================================================

static unsigned g_total = 0;
static unsigned g_passed = 0;

static void report(const char *name, bool ok) {
    g_total++;
    if (ok) g_passed++;
    printf("  [%s] %s\r\n", ok ? " OK " : "FAIL", name);
}

static void fill_pattern(uint8_t *buf, size_t n, uint8_t seed) {
    for (size_t i = 0; i < n; i++) buf[i] = (uint8_t) (seed + i);
}

// Compare buf to the same pattern fill_pattern() would produce.
// On mismatch, reports the first byte that differs.
static bool verify_pattern(const uint8_t *buf, size_t n, uint8_t seed,
                           const char *what) {
    for (size_t i = 0; i < n; i++) {
        uint8_t expected = (uint8_t) (seed + i);
        if (buf[i] != expected) {
            printf("       %s: mismatch at offset %u: got 0x%02x, expected 0x%02x\r\n",
                   what, (unsigned) i, buf[i], expected);
            return false;
        }
    }
    return true;
}

// Quick check that no bytes outside [0, n) were touched.
static bool verify_guard(const uint8_t *buf, size_t guard_size, uint8_t guard_val,
                         const char *what) {
    for (size_t i = 0; i < guard_size; i++) {
        if (buf[i] != guard_val) {
            printf("       %s: guard byte %u corrupted: 0x%02x\r\n",
                   what, (unsigned) i, buf[i]);
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
static constexpr size_t GUARD = 16;

static uint8_t *src_buf;
static uint8_t *dst_buf;


// ============================================================================
// Sanity: dump a few key registers so we can see the block is alive.
// ============================================================================

__unused static void dump_regs(const char *when) {
    // direct memory access; same address as in the driver
    auto rd = [](uint32_t addr) { return *(volatile uint32_t *) addr; };
    printf("  regs %s:\r\n", when);
    for (int ch = 0; ch < GDMA_NUM_CHANNELS; ch++) {
        printf("    ch%d.config = 0x%08lx\r\n", ch,
               (unsigned long) rd(0x00809000 + ch * 0x20));
    }
    printf("    int_status = 0x%08lx\r\n", (unsigned long) rd(0x00809000 + 0x38 * 4));
    printf("    prio_mode  = 0x%08lx\r\n", (unsigned long) rd(0x00809000 + 0x37 * 4));
}


// ============================================================================
// Local gdma_memcpy used by the tests.
// Lives in the test file (not the driver) - the driver intentionally exposes
// only the low-level API. Always uses dw=32/32 regardless of alignment: that
// gives BE=1111 on both sides and full bus throughput (4 bytes per dst write).
// Narrower widths are strictly slower for memcpy (see gdma_regs.h transfer
// model notes - they leave 3-byte gaps in dst between values).
// Returns dst on success, NULL on failure. n=0 is a no-op returning dst.
// Max single transfer is 262144 bytes (65536 dst writes * 4 bytes).
// ============================================================================
static void *gdma_memcpy(void *dst, const void *src, size_t n) {
    // return memcpy(dst, src, n);
    if (dst == nullptr || src == nullptr) {
        return nullptr;
    }
    if (n == 0) {
        return dst;
    }

    int ch = gdma_reserve_channel();
    if (ch < 0) {
        return nullptr;
    }

    gdma_config_t cfg = {
        .src = {
            .mode = GDMA_MODE_DTCM,
            .addr = (uint32_t) src,
            .incr = true,
            .dw = GDMA_DATA_WIDTH_32,
        },
        .dst = {
            .mode = GDMA_MODE_DTCM,
            .addr = (uint32_t) dst,
            .incr = true,
            .dw = GDMA_DATA_WIDTH_32,
        },
        .size = n,
    };

    int rc = gdma_run(ch, &cfg);
    if (rc != 0) {
        gdma_release_channel(ch);
        return nullptr;
    }

    gdma_wait(ch);
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
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;
    constexpr size_t N = 1024;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x10);

    void *r = gdma_memcpy(dst, src, N);
    bool ok = (r == dst)
              && verify_pattern(dst, N, 0x10, "aligned body")
              && verify_guard(dst_buf, GUARD, 0xAA, "aligned head guard")
              && verify_guard(dst_buf + GUARD + N, GUARD, 0xAA, "aligned tail guard");
    report("aligned src/dst, 1024 bytes", ok);
}

__unused static void test_align2() {
    // both aligned to 2 but not 4
    uint8_t *src = src_buf + GUARD + 2;
    uint8_t *dst = dst_buf + GUARD + 2;
    constexpr size_t N = 510;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x20);

    void *r = gdma_memcpy(dst, src, N);
    bool ok = (r == dst) && verify_pattern(dst, N, 0x20, "align2 body");
    report("src/dst aligned to 2, 510 bytes", ok);
}

__unused static void test_unaligned() {
    // odd offsets and odd length
    uint8_t *src = src_buf + GUARD + 1;
    uint8_t *dst = dst_buf + GUARD + 1;
    constexpr size_t N = 257;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x30);

    void *r = gdma_memcpy(dst, src, N);
    bool ok = (r == dst) && verify_pattern(dst, N, 0x30, "unaligned body");
    report("unaligned src/dst, 257 bytes", ok);
}

__unused static void test_mixed_alignment() {
    // src aligned to 4, dst aligned to 1
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD + 3;
    constexpr size_t N = 200;

    memset(dst_buf, 0xAA, GUARD + BUF_SIZE + GUARD);
    fill_pattern(src, N, 0x40);

    void *r = gdma_memcpy(dst, src, N);
    bool ok = (r == dst) && verify_pattern(dst, N, 0x40, "mixed body");
    report("mixed alignment (src@4, dst@1)", ok);
}

__unused static void test_patterns() {
    // Run a few stuck-bit patterns; helps catch a wired-OR or wired-AND bug
    // somewhere in the path.
    static const uint8_t patterns[] = {0x00, 0xFF, 0xAA, 0x55};
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;
    constexpr size_t N = 256;
    bool all_ok = true;

    for (uint8_t p: patterns) {
        memset(src, p, N);
        memset(dst, ~p, N);
        gdma_memcpy(dst, src, N);

        for (size_t i = 0; i < N; i++) {
            if (dst[i] != p) {
                printf("       pattern 0x%02x failed at %u: got 0x%02x\r\n",
                       p, (unsigned) i, dst[i]);
                all_ok = false;
                break;
            }
        }
    }
    report("stuck-bit patterns (0x00/0xFF/0xAA/0x55)", all_ok);
}

__unused static void test_large_transfer() {
    // Largest transfer that fits the buffer; checks that long bursts work.
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;
    constexpr size_t N = BUF_SIZE;

    memset(dst, 0, N);
    fill_pattern(src, N, 0x80);

    void *r = gdma_memcpy(dst, src, N);
    bool ok = (r == dst) && verify_pattern(dst, N, 0x80, "large body");
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
    void *r = gdma_memcpy(dst, src, 0);
    bool ok = (r == dst) && (dst[0] == 0xCC) && (dst[15] == 0xCC);
    report("zero-length is no-op", ok);
}

__unused static void test_oversize_rejected() {
    // Max transfer is 65536 dst writes * 4 bytes = 262144 bytes.
    // Pick a size strictly above that to force rejection.
    uint8_t *src = src_buf + GUARD;
    uint8_t *dst = dst_buf + GUARD;

    void *r = gdma_memcpy(dst, src, (size_t) 262144 + 4);
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
    bool ok = (gdma_memcpy(nullptr, src, 16) == nullptr)
              && (gdma_memcpy(dst, nullptr, 16) == nullptr);
    report("NULL src/dst rejected", ok);
}


// ============================================================================
// Benchmark
// ============================================================================

// Compiler barrier - prevents reordering of measured ops across the line.
#define COMPILER_BARRIER()      __asm__ volatile("" ::: "memory")


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
    return (uint32_t) ((uint64_t) bytes_total * 1000 / elapsed_us);
}

static void bench_one(const char *label, void *dst, const void *src,
                      size_t n, unsigned iters) {
    // warm-up: one call before each measurement to avoid first-iteration cost
    // dominating small-N timings.
    gdma_memcpy(dst, src, n);
    memcpy(dst, src, n);

    uint64_t t_dma = bench_gdma(dst, src, n, iters);
    uint64_t t_cpu = bench_memcpy(dst, src, n, iters);
    size_t total = n * iters;

    printf("  %-36s n=%5u x%-4u  gdma=%6lu us (%5lu KB/s)  memcpy=%6lu us (%5lu KB/s)\r\n",
           label, (unsigned) n, iters,
           (unsigned long) t_dma, (unsigned long) kb_per_sec(total, t_dma),
           (unsigned long) t_cpu, (unsigned long) kb_per_sec(total, t_cpu));
}

__unused static void run_benchmark() {
    printf("\r\n-- benchmark (gdma vs libc memcpy) --\r\n");

    uint8_t *src4 = src_buf + GUARD;     // aligned to 4
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

    run_benchmark();

    printf("\r\n%u / %u passed\r\n\r\n", g_passed, g_total);
}

int main() {
    wdt_down();

    platform_stdio_init();

    // disable stdout buffering so output is visible even if we crash
    setvbuf(stdout, NULL, _IONBF, 0);

    src_buf = new uint8_t[GUARD + BUF_SIZE + GUARD];
    dst_buf = new uint8_t[GUARD + BUF_SIZE + GUARD];

    wdt_set(10000);

    test_gdma();

    return 0;
}