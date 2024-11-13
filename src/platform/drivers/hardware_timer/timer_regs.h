#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/cdefs.h>


#define PWM_NEW_BASE                (0x00802A00)

#define TIMER_BANK_0                (PWM_NEW_BASE + 0x00 * 4)
#define TIMER_BANK_1                (PWM_NEW_BASE + 0x10 * 4)

#define TIMERS_IN_BANK              3
#define TIMERS_TOTAL                (TIMERS_IN_BANK * 2)

#define TIMER_BANK_0_FREQ           (26000000)
#define TIMER_BANK_1_FREQ           (32000)


typedef struct {
    uint32_t counter[3];

    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t enable: 3;
            uint32_t clk_divider: 4;
            uint32_t irq_status: 3;
        } bits;
    } ctl;

    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t read_op: 1;
            uint32_t _pad0: 1;
            uint32_t read_index: 2;
        } bits;
    } read_ctl;

    uint32_t read_value;
} hw_timer_bank_t;


#define hw_timer_bank0              ((volatile hw_timer_bank_t *)TIMER_BANK_0)
#define hw_timer_bank1              ((volatile hw_timer_bank_t *)TIMER_BANK_1)

static inline volatile hw_timer_bank_t *get_timer_bank_by_index(int timer_num) {
    return (timer_num < TIMERS_IN_BANK)
           ? hw_timer_bank0
           : hw_timer_bank1;
}

static inline int get_timer_num_in_bank_by_index(int timer_num) {
    return (timer_num < TIMERS_IN_BANK)
           ? (timer_num)
           : (timer_num - TIMERS_IN_BANK);
}

static inline uint32_t get_timer_frequency(int timer_num) {
    return (timer_num < TIMERS_IN_BANK) ? TIMER_BANK_0_FREQ : TIMER_BANK_1_FREQ;
}
