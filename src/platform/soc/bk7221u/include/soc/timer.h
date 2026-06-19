#pragma once

#include <stdbool.h>
#include "platform/soc.h"


#define PWM_NEW_BASE (0x00802A00)

#define TIMER_BANK_0 (PWM_NEW_BASE + 0x00 * 4)
#define TIMER_BANK_1 (PWM_NEW_BASE + 0x10 * 4)

#define TIMERS_IN_BANK 3
#define TIMERS_TOTAL   (TIMERS_IN_BANK * 2)

#define TIMER_BANK_0_FREQ (26000000)
#define TIMER_BANK_1_FREQ (32000)


typedef volatile struct {
    uint32_t counter[3]; // reload value for timer 0/1/2; also used as period

    union {
        uint32_t v;
        struct {
            uint32_t enable : 3;          // [2:0]   one bit per timer (timers 0/1/2 in bank), 1 = running
            uint32_t clk_divider : 4;     // [6:3]   clock pre-divider
            uint32_t irq_status : 3;      // [9:7]   one bit per timer; W1C
            uint32_t reserved_10_31 : 22; // [31:10]
        };
    } ctl;

    union {
        uint32_t v;
        struct {
            uint32_t read_op : 1;        // [0]     write 1 to latch current counter value into read_value
            uint32_t reserved_1 : 1;     // [1]
            uint32_t read_index : 2;     // [3:2]   which timer counter to latch (0/1/2)
            uint32_t reserved_4_31 : 28; // [31:4]
        };
    } read_ctl;

    uint32_t read_value; // latched counter snapshot; read after write to read_ctl.read_op
} hw_timer_bank_t;

#define hw_timer_bank0 ((volatile hw_timer_bank_t *)TIMER_BANK_0)
#define hw_timer_bank1 ((volatile hw_timer_bank_t *)TIMER_BANK_1)

static inline volatile hw_timer_bank_t *get_timer_bank_by_index(int timer_num) {
    return (timer_num < TIMERS_IN_BANK) ? hw_timer_bank0 : hw_timer_bank1;
}

static inline int get_timer_num_in_bank_by_index(int timer_num) {
    return (timer_num < TIMERS_IN_BANK) ? (timer_num) : (timer_num - TIMERS_IN_BANK);
}

static inline uint32_t get_timer_frequency(int timer_num) {
    return (timer_num < TIMERS_IN_BANK) ? TIMER_BANK_0_FREQ : TIMER_BANK_1_FREQ;
}
