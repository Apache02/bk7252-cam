#pragma once

#include "platform/soc.h"


// PWM block base — at PWM_NEW_BASE + 0x20 * 4 (timer banks occupy 0x00 and 0x10)
#define PWM_BASE_ADDR  (0x00802A80)
#define PWM_CHANNELS   (6)

// mode field values (2 bits, see hw_pwm_t.ctl.chN_mode)
#define PWM_MODE_PWM      (0)  // output signal on pin; output high [0..duty), low [duty..period)
#define PWM_MODE_TIMER    (1)  // interrupt-only, no pin output
#define PWM_MODE_CAP_POS  (2)  // capture: latch counter on rising edge
#define PWM_MODE_CAP_NEG  (3)  // capture: latch counter on falling edge


typedef volatile struct {
    uint32_t period;  // counter wraps back to 0 (and fires IRQ if int_en) when it reaches this value
    uint32_t duty;    // output goes low when counter reaches this; 0 = always low, ≥period = always high
    uint32_t cap;     // latched counter value on pin edge (capture mode only)
} hw_pwm_ch_t;


typedef volatile struct {
    // Each channel occupies 4 bits: [en, int_en, mode[1:0]] at bits [4n+3:4n]
    union {
        uint32_t v;
        struct {
            uint32_t ch0_en : 1;          // [0]
            uint32_t ch0_int_en : 1;      // [1]
            uint32_t ch0_mode : 2;        // [3:2]
            uint32_t ch1_en : 1;          // [4]
            uint32_t ch1_int_en : 1;      // [5]
            uint32_t ch1_mode : 2;        // [7:6]
            uint32_t ch2_en : 1;          // [8]
            uint32_t ch2_int_en : 1;      // [9]
            uint32_t ch2_mode : 2;        // [11:10]
            uint32_t ch3_en : 1;          // [12]
            uint32_t ch3_int_en : 1;      // [13]
            uint32_t ch3_mode : 2;        // [15:14]
            uint32_t ch4_en : 1;          // [16]
            uint32_t ch4_int_en : 1;      // [17]
            uint32_t ch4_mode : 2;        // [19:18]
            uint32_t ch5_en : 1;          // [20]
            uint32_t ch5_int_en : 1;      // [21]
            uint32_t ch5_mode : 2;        // [23:22]
            uint32_t reserved_24_31 : 8;  // [31:24]
        };
    } ctl;

    union {
        uint32_t v;
        struct {
            uint32_t ch0 : 1;             // [0]   write 1 to clear
            uint32_t ch1 : 1;             // [1]
            uint32_t ch2 : 1;             // [2]
            uint32_t ch3 : 1;             // [3]
            uint32_t ch4 : 1;             // [4]
            uint32_t ch5 : 1;             // [5]
            uint32_t reserved_6_31 : 26;  // [31:6]
        };
    } int_status;

    hw_pwm_ch_t ch[PWM_CHANNELS];
} hw_pwm_t;

#define hw_pwm  ((volatile hw_pwm_t *)PWM_BASE_ADDR)
