#include "shell/commands_beken.h"
#include "shell/console_colors.h"
#include "shell/Parser.h"
#include <stdio.h>
#include <string.h>
#include "hardware/time.h"
#include "platform/cpu.h"
#include "soc/sctrl.h"


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


#define MCLK_FIELD_DCO                      (0x0)
#define MCLK_FIELD_26M_XTAL                 (0x1)
#define MCLK_FIELD_DPLL                     (0x2)
#define MCLK_FIELD_LPO                      (0x3)

struct id_name_map {
    int id;
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
        printf(
            "    source: %s %s %s %s\r\n",
            mclk_name_map[0].name,
            mclk_name_map[1].name,
            mclk_name_map[2].name,
            mclk_name_map[3].name
        );
        printf("\r\n");

        return 0;
    }

    typeof(hw_sctrl->control) tmp;
    tmp.v = hw_sctrl->control.v;

    int set_source = -1;
    if (argc > 1) {
        set_source = get_id_by_name(mclk_name_map, argv[1]);

        if (set_source < 0) {
            printf(COLOR_RED("Error: unknown source") "\r\n");
            printf(
                "Expected: %s %s %s %s\r\n",
                mclk_name_map[0].name,
                mclk_name_map[1].name,
                mclk_name_map[2].name,
                mclk_name_map[3].name
            );

            return 1;
        }
    }

    int set_divider = -1;
    if (argc > 2) {
        set_divider = take_int(argv[2]).ok_or(-1);

        if (set_divider < 0 || set_divider > 15) {
            printf(COLOR_RED("Error: divider must be in range 0..15") "\r\n");
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
