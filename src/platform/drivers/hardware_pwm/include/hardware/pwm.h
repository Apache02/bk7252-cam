#pragma once

#include <stdint.h>


// PWM / timer driver for the BK7221U PWM block (6 channels, base 0x00802A80).
//
// Each channel can run in one of two modes:
//   output — drives a PWM signal on a pin (caller configures ICU GPIO mux)
//   timer  — fires a callback each period, no pin involved
//
// Period and duty are raw clock ticks. Clock source per channel is selected via
// ICU pwm_clk_mux (PCLK or LPO 32 kHz); default at power-on is PCLK.
//
// Channels 0–5 are independent. Using the same channel from both pwm_output_*
// and pwm_timer_* simultaneously is undefined behaviour.

typedef void (*pwm_timer_cb_t)(void);

// --- PWM output mode ---

// Configure channel ch and start PWM output.
// duty must be < period; duty=0 gives a constant-low signal.
void pwm_output_init(unsigned ch, uint32_t period, uint32_t duty);

// Update duty cycle without stopping the channel.
void pwm_output_set_duty(unsigned ch, uint32_t duty);

// Stop PWM output and power down the channel clock.
void pwm_output_deinit(unsigned ch);

// --- Timer mode ---

// Configure channel ch as a periodic timer; cb fires in IRQ context each period.
void pwm_timer_init(unsigned ch, uint32_t period, pwm_timer_cb_t cb);

// Stop timer and power down the channel clock.
void pwm_timer_deinit(unsigned ch);
