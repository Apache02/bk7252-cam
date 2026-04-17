#include "shell/commands_beken.h"
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include <stdio.h>
#include "hardware/time.h"
#include "hardware/cpu.h"
#include "hardware/sctrl_regs.h"


int command_cpu_speed(int argc, const char *argv[]) {
    // ~33 ms window at 120 MHz — well above the 1 us timer resolution.
    uint32_t iterations = argc == 2
                              ? static_cast<uint32_t>(take_int(argv[1]).ok_or(0))
                              : 1000000;
    if (iterations < 10000) {
        printf(COLOR_RED("Error: iterations must be >= 10000") "\r\n");
        return 1;
    }

    uint32_t loop = iterations;
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    absolute_time_t start = get_absolute_time();
    // ARM968E-S Thumb: subs(1 cycle) + bne taken(3 cycles, pipeline refill) = 4 cycles/iter.
    __asm volatile(
        ".syntax unified\n"
        "1: subs %0, #1\n"
        "bne 1b\n"
        : "+l"(loop) : : "cc", "memory"
    );
    int64_t us_spend = absolute_time_diff_us(start, get_absolute_time());
    GLOBAL_INT_RESTORE();

    if (us_spend <= 0) {
        printf(COLOR_RED("Error: timing failed (us=%ld)") "\r\n", static_cast<long>(us_spend));
        return 1;
    }

    uint64_t cycles = static_cast<uint64_t>(iterations) * 4ULL;
    uint64_t hz = cycles * 1'000'000ULL / static_cast<uint64_t>(us_spend);

    printf("Iterations: %lu\r\n", static_cast<unsigned long>(iterations));
    printf("Cycles:     %lu\r\n", static_cast<unsigned long>(cycles));
    printf("Elapsed:    %lu us\r\n", static_cast<unsigned long>(us_spend));
    printf(
        "CPU freq:   %lu Hz (%lu.%02lu MHz)\r\n",
        static_cast<unsigned long>(hz),
        static_cast<unsigned long>(hz / 1000000),
        static_cast<unsigned long>((hz % 1000000) / 10000)
    );

    return 0;
}

static const char *mclk_source_name(uint32_t s) {
    switch (s) {
        case 0: return "DCO";
        case 1: return "26M_XTAL";
        case 2: return "DPLL";
        case 3: return "LPO";
        default: return "?";
    }
}

int command_mclk_source(int argc, const char *argv[]) {
    if (argc < 2) {
        uint32_t s = hw_sctrl->control.mclk_source;
        printf("MCLK source: %lu (%s)\r\n", static_cast<unsigned long>(s), mclk_source_name(s));
        printf("  0=DCO  1=26M_XTAL  2=DPLL  3=LPO\r\n");
        return 0;
    }

    int v = take_int(argv[1]).ok_or(-1);
    if (v < 0 || v > 3) {
        printf(COLOR_RED("Error: source must be 0..3 (0=DCO, 1=26M_XTAL, 2=DPLL, 3=LPO)") "\r\n");
        return 1;
    }

    // Read-modify-write the full 32-bit word — never poke individual bit-fields on a volatile reg.
    typeof(hw_sctrl->control) tmp;
    tmp.v = hw_sctrl->control.v;
    tmp.mclk_source = static_cast<uint32_t>(v);
    hw_sctrl->control.v = tmp.v;

    printf("MCLK source set to %d (%s)\r\n", v, mclk_source_name(static_cast<uint32_t>(v)));
    return 0;
}

int command_mclk_divider(int argc, const char *argv[]) {
    if (argc < 2) {
        uint32_t d = hw_sctrl->control.divider;
        printf("MCLK divider: %lu\r\n", static_cast<unsigned long>(d));
        return 0;
    }

    int v = take_int(argv[1]).ok_or(-1);
    if (v < 0 || v > 15) {
        printf(COLOR_RED("Error: divider must be 0..15") "\r\n");
        return 1;
    }

    typeof(hw_sctrl->control) tmp;
    tmp.v = hw_sctrl->control.v;
    tmp.divider = static_cast<uint32_t>(v);
    hw_sctrl->control.v = tmp.v;

    printf("MCLK divider set to %d\r\n", v);
    return 0;
}
