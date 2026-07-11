#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hardware/time.h"
#include "hardware/timer.h"
#include "hardware/wdt.h"
#include "platform/cpu.h"
#include "soc/sctrl.h"

#define MCLK_FIELD_DCO      (0x0)
#define MCLK_FIELD_26M_XTAL (0x1)
#define MCLK_FIELD_DPLL     (0x2)
#define MCLK_FIELD_LPO      (0x3)

// subs+bne loop, 4 cycles/iteration. Position-independent (PC-relative
// branch), so it can be copied to TCM (zero-wait-state, see
// docs/memory_map.md) and run from there instead of wherever this command
// is linked — --no_tcm skips that relocation.
extern "C" __attribute__((naked, noinline)) void cpu_bench_loop(uint32_t iterations) {
    __asm volatile(".syntax unified\n"
                   "1: subs r0, r0, #1\n"
                   "bne 1b\n"
                   "bx lr\n"
                   ".global cpu_bench_loop_end\n"
                   "cpu_bench_loop_end:\n");
}
extern "C" const uint8_t cpu_bench_loop_end[];

typedef void (*bench_fn_t)(uint32_t);

#define TCM_SCRATCH_ADDR 0x003F8000u
#define TCM_SCRATCH_SIZE 32u

// Mask the Thumb interworking bit before subtracting — cpu_bench_loop_end
// is a bare label and may not carry it the way cpu_bench_loop does.
static bool bench_loop_bounds(uintptr_t *raw_start, uint32_t *code_size) {
    uintptr_t start = reinterpret_cast<uintptr_t>(cpu_bench_loop) & ~static_cast<uintptr_t>(1);
    uintptr_t end   = reinterpret_cast<uintptr_t>(cpu_bench_loop_end) & ~static_cast<uintptr_t>(1);
    uint32_t  size  = static_cast<uint32_t>(end - start);
    if (size == 0 || size > TCM_SCRATCH_SIZE) {
        return false;
    }
    *raw_start = start;
    *code_size = size;
    return true;
}

// Backs up the TCM scratch bytes, writes the loop, returns a callable
// pointer. tcm_bench_exit() restores the backup afterward.
static bench_fn_t tcm_bench_enter(uint8_t **out_backup) {
    uintptr_t raw_start;
    uint32_t  code_size;
    if (!bench_loop_bounds(&raw_start, &code_size)) {
        return nullptr;
    }

    uint8_t *backup = static_cast<uint8_t *>(malloc(TCM_SCRATCH_SIZE));
    if (!backup) {
        return nullptr;
    }

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    memcpy(backup, reinterpret_cast<void *>(TCM_SCRATCH_ADDR), TCM_SCRATCH_SIZE);
    memcpy(reinterpret_cast<void *>(TCM_SCRATCH_ADDR), reinterpret_cast<void *>(raw_start), code_size);
    GLOBAL_INT_RESTORE();

    *out_backup = backup;
    return reinterpret_cast<bench_fn_t>(TCM_SCRATCH_ADDR | 1u);
}

static void tcm_bench_exit(uint8_t *backup) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    memcpy(reinterpret_cast<void *>(TCM_SCRATCH_ADDR), backup, TCM_SCRATCH_SIZE);
    GLOBAL_INT_RESTORE();
    free(backup);
}

// Default timebase: NXMAC counter, 1 us resolution. Hangs the CPU if mclk
// is VCO or DPLL with a large divider — use --use_timer there instead.
static int64_t time_via_nxmac(bench_fn_t fn, uint32_t iterations) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    absolute_time_t start = get_absolute_time();
    fn(iterations);
    int64_t us_spend = absolute_time_diff_us(start, get_absolute_time());
    GLOBAL_INT_RESTORE();
    return us_spend;
}

#define TIMER_TICK_FREQ_HZ 1000u // 1 ms/tick

static volatile uint32_t g_tick_count;

static void cpu_speed_tick_isr(int timer_num) {
    (void)timer_num;
    g_tick_count = g_tick_count + 1;
}

// Below this many elapsed ticks, the +-1 tick quantization error exceeds 5%
// and the reading is not trustworthy — see time_via_timer().
#define TIMER_MIN_RELIABLE_TICKS 20u

// --use_timer timebase: Timer Bank 0, 26 MHz, independent of mclk. Pauses
// every other timer channel (e.g. FreeRTOS/RTC tick) so only ours can fire.
// Needs global IRQs enabled, unlike time_via_nxmac — coarser too (+-1 tick,
// 1 ms), so out_ticks lets the caller warn on too few ticks.
static int64_t time_via_timer(bench_fn_t fn, uint32_t iterations, uint32_t *out_ticks) {
    g_tick_count = 0;

    int timer_num = timer_create_by_freq(TIMER_TICK_FREQ_HZ, cpu_speed_tick_isr, false);
    if (timer_num < 0) {
        return -1;
    }

    uint32_t paused_mask = timer_pause_all();
    timer_start(timer_num);

    fn(iterations);

    uint32_t ticks = g_tick_count;
    timer_remove(timer_num);
    timer_resume_all(paused_mask);

    *out_ticks = ticks;
    return static_cast<int64_t>(ticks) * static_cast<int64_t>(1000000u / TIMER_TICK_FREQ_HZ);
}

static void print_cpu_speed_usage(const char *argv0) {
    printf("\r\n");
    printf("Usage:\r\n");
    printf("    %s [iterations] [--no_tcm] [--use_timer | --no_timer] [--pause_wdg]\r\n", argv0);
    printf("\r\n");
    printf("    iterations:    loop iteration count (default 1000000, min 10000)\r\n");
    printf("    --no_tcm:      run the loop in place instead of relocating it to TCM\r\n");
    printf("                   (result becomes region-dependent — see docs/memory_map.md)\r\n");
    printf("    --use_timer:   time via Timer Bank 0 (26 MHz, mclk-independent) instead of\r\n");
    printf("                   the NXMAC counter; coarser (+-1 tick, 1 ms) — use a larger\r\n");
    printf("                   iteration count to compensate\r\n");
    printf("    --no_timer:    force the NXMAC counter even though mclk source is not DPLL\r\n");
    printf("                   (may hang the CPU — see docs/memory_map.md)\r\n");
    printf("    --pause_wdg:   temporarily disable the watchdog for the test duration\r\n");
    printf("\r\n");
    printf("    mclk source must be DPLL to use the default (NXMAC) timebase; any other\r\n");
    printf("    source requires --use_timer or an explicit --no_timer override.\r\n");
    printf("\r\n");
}

int command_cpu_speed(int argc, const char *argv[]) {
    // ~33 ms window at 120 MHz — well above the NXMAC timebase's 1 us resolution.
    uint32_t iterations = 1000000;
    bool     no_tcm     = false;
    bool     use_timer  = false;
    bool     no_timer   = false;
    bool     pause_wdg  = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_cpu_speed_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--no_tcm") == 0) {
            no_tcm = true;
        } else if (strcmp(argv[i], "--use_timer") == 0) {
            use_timer = true;
        } else if (strcmp(argv[i], "--no_timer") == 0) {
            no_timer = true;
        } else if (strcmp(argv[i], "--pause_wdg") == 0) {
            pause_wdg = true;
        } else {
            iterations = static_cast<uint32_t>(take_int(argv[i]).ok_or(0));
        }
    }

    if (iterations < 10000) {
        printf("Error: iterations must be >= 10000\r\n");
        return 1;
    }

    uint32_t mclk_source = hw_sctrl->control.mclk_source;
    if (mclk_source != MCLK_FIELD_DPLL && !use_timer && !no_timer) {
        printf("Error: mclk source is not DPLL (source=%lu) — the NXMAC counter may hang here.\r\n",
               static_cast<unsigned long>(mclk_source));
        printf("Pass --use_timer (safe) or --no_timer (force NXMAC anyway) to proceed.\r\n");
        return 1;
    }

    bool wdg_was_active = false;
    if (pause_wdg) {
        wdg_was_active = wdt_is_active() != 0;
        if (wdg_was_active) {
            wdt_down();
        }
    }

    bench_fn_t fn;
    uint8_t   *tcm_backup = nullptr;

    if (no_tcm) {
        fn = cpu_bench_loop;
    } else {
        fn = tcm_bench_enter(&tcm_backup);
        if (!fn) {
            printf("Error: failed to relocate bench loop to TCM\r\n");
            if (pause_wdg && wdg_was_active) {
                wdt_up();
            }
            return 1;
        }
    }

    uint32_t ticks    = 0;
    int64_t  us_spend = use_timer ? time_via_timer(fn, iterations, &ticks) : time_via_nxmac(fn, iterations);

    if (!no_tcm) {
        tcm_bench_exit(tcm_backup);
    }

    if (pause_wdg && wdg_was_active) {
        wdt_up();
    }

    if (use_timer && us_spend < 0) {
        printf("Error: failed to allocate a timer channel\r\n");
        return 1;
    }
    if (us_spend <= 0) {
        printf("Error: timing failed (us=%ld)\r\n", static_cast<long>(us_spend));
        return 1;
    }
    if (use_timer && ticks < TIMER_MIN_RELIABLE_TICKS) {
        printf("Warning: only %lu tick(s) elapsed (+-%lu%% error) — pass a larger iteration count\r\n",
               static_cast<unsigned long>(ticks), static_cast<unsigned long>(100u / (ticks ? ticks : 1u)));
    }

    // ARM968E-S Thumb: subs(1 cycle) + bne taken(3 cycles, pipeline refill) = 4 cycles/iter.
    uint64_t      cycles    = static_cast<uint64_t>(iterations) * 4ULL;
    uint64_t      hz        = cycles * 1'000'000ULL / static_cast<uint64_t>(us_spend);
    unsigned long cpu_hz    = static_cast<unsigned long>(hz);
    unsigned long cpu_mhz   = static_cast<unsigned long>(hz / 1000000);
    unsigned long cpu_mhz_i = static_cast<unsigned long>((hz % 1000000) / 10000);

    const char *const location_label =
        no_tcm ? "in-place (region-dependent)" : "TCM-relocated (region-independent)";
    const char *const timer_source_label =
        use_timer ? "Timer Bank 0, 26 MHz (mclk-independent)" : "NXMAC counter (hangs under some mclk configs)";

    printf("Location:   %s\r\n", location_label);
    printf("Timebase:   %s\r\n", timer_source_label);
    printf("Iterations: %lu\r\n", static_cast<unsigned long>(iterations));
    printf("Cycles:     %lu\r\n", static_cast<unsigned long>(cycles));
    printf("Elapsed:    %lu us\r\n", static_cast<unsigned long>(us_spend));
    printf("CPU freq:   %lu Hz (%lu.%02lu MHz)\r\n", cpu_hz, cpu_mhz, cpu_mhz_i);
    printf("\r\n");

    return 0;
}

struct id_name_map {
    int         id;
    const char *name;
};

static const id_name_map mclk_name_map[] = {
    {MCLK_FIELD_DCO, "DCO"},
    {MCLK_FIELD_26M_XTAL, "26M_XTAL"},
    {MCLK_FIELD_DPLL, "DPLL"},
    {MCLK_FIELD_LPO, "LPO"},
    {-1, ""},
};

static const char *get_name_by_id(const id_name_map *map, int id) {
    for (int i = 0; map[i].id != -1; i++) {
        if (map[i].id == id) {
            return map[i].name;
        }
    }

    return "unknown";
}

static int get_id_by_name(const id_name_map *map, const char *name) {
    for (int i = 0; map[i].id != -1; i++) {
        if (strcasecmp(name, map[i].name) == 0) return map[i].id;
    }

    return -1;
}

int command_mclk(int argc, const char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--help") == 0) {
        printf("\r\n");
        printf("Usage:\r\n");
        printf("    %s <source> [divider]\r\n", argv[0]);
        printf("\r\n");
        printf("    source: %s %s %s %s\r\n", mclk_name_map[0].name, mclk_name_map[1].name, mclk_name_map[2].name,
               mclk_name_map[3].name);
        printf("\r\n");

        return 0;
    }

    typeof(hw_sctrl->control) tmp;
    tmp.v = hw_sctrl->control.v;

    int set_source = -1;
    if (argc > 1) {
        set_source = get_id_by_name(mclk_name_map, argv[1]);

        if (set_source < 0) {
            printf("Error: unknown source\r\n");
            printf("Expected: %s %s %s %s\r\n", mclk_name_map[0].name, mclk_name_map[1].name, mclk_name_map[2].name,
                   mclk_name_map[3].name);

            return 1;
        }
    }

    int set_divider = -1;
    if (argc > 2) {
        set_divider = take_int(argv[2]).ok_or(-1);

        if (set_divider < 0 || set_divider > 15) {
            printf("Error: divider must be in range 0..15\r\n");
            return 2;
        }
    }

    if (set_source != -1 || set_divider != -1) {
        if (set_source != -1) {
            tmp.mclk_source = set_source;
        }
        if (set_divider != -1) {
            tmp.divider = set_divider;
        }
        hw_sctrl->control.v = tmp.v;
    }

    printf("   source:  %d (%s)\r\n", tmp.mclk_source, get_name_by_id(mclk_name_map, tmp.mclk_source));
    printf("  divider: %d\r\n", tmp.divider);

    return 0;
}
