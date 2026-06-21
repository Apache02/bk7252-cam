#include "hardware/pwm.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "soc/pwm.h"

static pwm_timer_cb_t g_timer_cbs[PWM_CHANNELS];

// peri_clk_pwd bits for pwm0..pwm5 are contiguous at bits 9..14
static void pwm_clock_up(unsigned ch)   { hw_icu->peri_clk_pwd.v &= ~(1u << (9 + ch)); }
static void pwm_clock_down(unsigned ch) { hw_icu->peri_clk_pwd.v |=  (1u << (9 + ch)); }

// CTL layout: 4 bits per channel at [4n+3:4n] — [en, int_en, mode[1], mode[0]]
static void pwm_ch_enable(unsigned ch, uint32_t mode, int int_en) {
    uint32_t shift = ch * 4;
    uint32_t ctl   = hw_pwm->ctl.v & ~(0xFu << shift);
    ctl |= (1u | ((uint32_t)!!int_en << 1) | (mode << 2)) << shift;
    hw_pwm->ctl.v = ctl;
}

static void pwm_ch_disable(unsigned ch) {
    hw_pwm->ctl.v &= ~(0xFu << (ch * 4));
}

static void pwm_isr(void) {
    uint32_t fired = hw_pwm->int_status.v;
    hw_pwm->int_status.v = fired;

    for (unsigned i = 0; i < PWM_CHANNELS; i++) {
        if ((fired & (1u << i)) && g_timer_cbs[i])
            g_timer_cbs[i]();
    }
}

// --- PWM output ---

void pwm_output_init(unsigned ch, uint32_t period, uint32_t duty) {
    pwm_clock_up(ch);
    hw_pwm->ch[ch].period = period;
    hw_pwm->ch[ch].duty   = duty;
    pwm_ch_enable(ch, PWM_MODE_PWM, 0);
}

void pwm_output_set_duty(unsigned ch, uint32_t duty) {
    hw_pwm->ch[ch].duty = duty;
}

void pwm_output_deinit(unsigned ch) {
    pwm_ch_disable(ch);
    pwm_clock_down(ch);
}

// --- Timer mode ---

void pwm_timer_init(unsigned ch, uint32_t period, pwm_timer_cb_t cb) {
    g_timer_cbs[ch] = cb;
    pwm_clock_up(ch);
    hw_pwm->ch[ch].period = period;
    hw_pwm->ch[ch].duty   = 0;
    intc_register_irq_handler(IRQ_SOURCE_PWM, pwm_isr);
    intc_enable_irq_source(IRQ_SOURCE_PWM);
    pwm_ch_enable(ch, PWM_MODE_TIMER, 1);
}

void pwm_timer_deinit(unsigned ch) {
    pwm_ch_disable(ch);
    g_timer_cbs[ch] = 0;
    pwm_clock_down(ch);
}
