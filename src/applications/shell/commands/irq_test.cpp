#include "commands.h"
#include <stdio.h>

#include "utils/busy_wait.h"
#include "hardware/intc.h"
#include "hardware/cpu.h"


#define TRACE(var)                          printf(#var " = 0x%x\r\n", var)
#define TRACE_CHANGE(var, new_value)        printf(#var ": 0x%x -> 0x%x\r\n", var, new_value)


#define PWM_NEW_BASE                                 (0x00802A00)
#define PWM_BASE                                     (PWM_NEW_BASE + 0x20 * 4 )

#define PWM_INTERRUPT_STATUS_MASK                    (0b111111)


typedef struct {
    union {
        uint32_t reg;
        struct __packed {
            uint32_t pwm0_enable: 1;
            uint32_t pwm0_int_enable: 1;
            uint32_t pwm0_mode: 2;
            uint32_t pwm1_enable: 1;
            uint32_t pwm1_int_enable: 1;
            uint32_t pwm1_mode: 2;
            uint32_t pwm2_enable: 1;
            uint32_t pwm2_int_enable: 1;
            uint32_t pwm2_mode: 2;
            uint32_t pwm3_enable: 1;
            uint32_t pwm3_int_enable: 1;
            uint32_t pwm3_mode: 2;
            uint32_t pwm4_enable: 1;
            uint32_t pwm4_int_enable: 1;
            uint32_t pwm4_mode: 2;
            uint32_t pwm5_enable: 1;
            uint32_t pwm5_int_enable: 1;
            uint32_t pwm5_mode: 2;
        };
    } ctl;

    union {
        uint32_t reg;
        struct __packed {
            uint32_t pwm0: 1;
            uint32_t pwm1: 1;
            uint32_t pwm2: 1;
            uint32_t pwm3: 1;
            uint32_t pwm4: 1;
            uint32_t pwm5: 1;
        };
    } int_status;

    struct {
        uint32_t counter;
        uint32_t duty;
        uint32_t capture;
    } params[6];
} hw_pwm_t;


#define hw_pwm      ((volatile hw_pwm_t *)PWM_BASE)


static void pwm_isr() {
    auto value = hw_pwm->int_status.reg & PWM_INTERRUPT_STATUS_MASK;

    do {
        // todo call handler here
        printf("i");
        hw_pwm->int_status.reg = 1;
    } while (hw_pwm->int_status.reg & 1);

    do {
        hw_pwm->int_status.reg = value;
    } while (hw_pwm->int_status.reg & value);
}

void command_pwm(Console &c) {
    intc_enable_irq_source(IRQ_SOURCE_PWM);

    hw_pwm->params[0].counter = 60'000'000;
    hw_pwm->ctl.pwm0_enable = 1;
    hw_pwm->ctl.pwm0_int_enable = 1;

    intc_register_irq_handler(IRQ_SOURCE_PWM, pwm_isr);

    printf("enable irq\r\n");
    portENABLE_IRQ();
//    portENABLE_FIQ();

    for (auto i = 0; i < 200; i++) {
        busy_wait_ms(10);
        putchar('.');
    }

    portDISABLE_IRQ();
//    portDISABLE_FIQ();
    printf("disabled irq\r\n");

    intc_unregister_irq_handler(IRQ_SOURCE_PWM, pwm_isr);
}

void command_pwm_stop(Console &c) {
    hw_pwm->ctl.pwm0_int_enable = 0;
    hw_pwm->ctl.pwm0_enable = 0;
}
