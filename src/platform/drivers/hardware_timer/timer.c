#include "hardware/timer.h"
#include "timer_regs.h"
#include "sys_counter.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "utils/assert.h"


typedef struct {
    timer_alarm_handler_t *handler;
    uint32_t type;
} hw_timer_t;

enum {
    TYPE_NONE = 0,
    TYPE_ONCE = 1,
    TYPE_PERIODIC = 2,
};

static hw_timer_t timers_handlers[TIMERS_TOTAL] = {0};


static void timer_irq_handler(interrupt_context_t context) {
    uint32_t status0 = hw_timer_bank0->ctl.bits.int_status;
    uint32_t status1 = hw_timer_bank1->ctl.bits.int_status;
    uint32_t status_all = status0 | (status1 << TIMERS_IN_BANK);

    for (int i = 0; i < TIMERS_TOTAL; i++) {
        if (!(status_all & (1 << i))) continue;

        timer_alarm_handler_t *handler_func = timers_handlers[i].handler;

        if (handler_func) {
            handler_func(i);
        }

        if (timers_handlers[i].type == TYPE_ONCE) {
            timers_handlers[i].handler = NULL;
            timers_handlers[i].type = TYPE_NONE;
        }
    }

    // clear timers
    if (status0) {
        do {
            hw_timer_bank0->ctl.bits.int_status = status0;
        } while (hw_timer_bank0->ctl.bits.int_status & status0);
    }
    if (status1) {
        do {
            hw_timer_bank1->ctl.bits.int_status = status1;
        } while (hw_timer_bank1->ctl.bits.int_status & status1);
    }
}

static int find_free_timer() {
    for (int i = 0; i < TIMERS_TOTAL; i++) {
        if (timers_handlers[i].type == TYPE_NONE) {
            return i;
        }
    }

    return -1;
}

void timer_init() {
    hw_icu->peri_clk_pwd.bits.timer_26m = 0;
    hw_icu->peri_clk_pwd.bits.tl410_wdt = 0;

    // start sys counter on timer #0
    timers_handlers[0].type = TYPE_PERIODIC;
    timers_handlers[0].handler = &sys_counter_tick;

    hw_timer_bank0->counter[0] = 26000000;
    hw_timer_bank0->ctl.bits.clk_divider = 0;
    hw_timer_bank0->ctl.bits.int_status = (1 << 0);
    hw_timer_bank0->ctl.bits.enable |= (1 << 0);

    intc_register_irq_handler(IRQ_SOURCE_TIMER, timer_irq_handler);
    intc_enable_irq_source(IRQ_SOURCE_TIMER);
}

int timer_create(bool once, uint32_t count, int divider, timer_alarm_handler_t *func) {
    assert_true(divider >= 1 && divider <= 8, "Invalid divider");

    int timer_num = find_free_timer();
    if (timer_num < 0) return -1;

    timers_handlers[timer_num].type = once ? TYPE_ONCE : TYPE_PERIODIC;
    timers_handlers[timer_num].handler = func;

    hw_timer_bank0->counter[0] = count;
    hw_timer_bank0->ctl.bits.clk_divider = divider - 1;
    hw_timer_bank0->ctl.bits.int_status &= ~(1 << timer_num);
    hw_timer_bank0->ctl.bits.enable |= (1 << timer_num);

    return timer_num;
}

void timer_remove(int timer_num) {
    assert_true(timer_num >= 0 && timer_num <= (TIMERS_TOTAL-1), "Invalid timer number");

    hw_timer_bank0->ctl.bits.enable &= ~(1 << timer_num);

    timers_handlers[timer_num].type = TYPE_NONE;
    timers_handlers[timer_num].handler = NULL;
}


//int busy_wait() {
//}
//
//int busy_wait_ms() {
//}
