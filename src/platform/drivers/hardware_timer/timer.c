#include "hardware/timer.h"
#include "soc/timer.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "platform/cpu.h"
#include "platform/init.h"

typedef struct {
    timer_alarm_handler_t *handler;
    uint32_t               type;
} private_timer_t;

enum {
    TYPE_NONE     = 0,
    TYPE_ONCE     = 1,
    TYPE_PERIODIC = 2,
};

static private_timer_t timers_handlers[TIMERS_TOTAL] = {0};

#define assert_timer_number(i) assert((i >= 0) && (i < TIMERS_TOTAL))

#define timer_clear_irq_status(bank, status) \
    while (bank->ctl.irq_status & status) {  \
        bank->ctl.irq_status = status;       \
    }

// irq_status shares the ctl word and is W1C, so a plain read-modify-write of any other
// field writes pending flags back as 1 and clears them. Write them as 0 instead.
static void timer_bank_update_enable(volatile hw_timer_bank_t *bank, uint32_t set, uint32_t clear) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    typeof(bank->ctl) ctl = {.v = bank->ctl.v};
    ctl.irq_status        = 0;
    ctl.enable            = (ctl.enable | set) & ~clear;
    bank->ctl.v           = ctl.v;

    GLOBAL_INT_RESTORE();
}


static void timer_isr() {
    uint32_t status0    = hw_timer_bank0->ctl.irq_status;
    uint32_t status1    = hw_timer_bank1->ctl.irq_status;
    uint32_t status_all = status0 | (status1 << TIMERS_IN_BANK);

    for (int i = 0; i < TIMERS_TOTAL; i++) {
        if (!(status_all & (1 << i))) continue;

        timer_alarm_handler_t *handler_func = timers_handlers[i].handler;

        if (handler_func) {
            handler_func(i);
        }

        if (timers_handlers[i].type == TYPE_ONCE) {
            volatile hw_timer_bank_t *bank              = get_timer_bank_by_index(i);
            int                       timer_num_in_bank = get_timer_num_in_bank_by_index(i);
            timer_bank_update_enable(bank, 0, 1u << timer_num_in_bank);

            timers_handlers[i].handler = NULL;
            timers_handlers[i].type    = TYPE_NONE;
        }
    }

    if (status0) timer_clear_irq_status(hw_timer_bank0, status0);
    if (status1) timer_clear_irq_status(hw_timer_bank1, status1);
}

static int find_free_timer(uint32_t freq) {
    for (int timer_num = TIMERS_TOTAL - 1; timer_num >= 0; timer_num--) {
        const int timer_clock_freq = get_timer_frequency(timer_num);
        if (freq != 0 && freq > timer_clock_freq) {
            continue;
        }

        if (timers_handlers[timer_num].type == TYPE_NONE) {
            return timer_num;
        }
    }

    return -1;
}

void timer_reset(void) {
    hw_timer_bank0->ctl.v = 0;
    hw_timer_bank1->ctl.v = 0;

    timer_clear_irq_status(hw_timer_bank0, hw_timer_bank0->ctl.irq_status);
    timer_clear_irq_status(hw_timer_bank1, hw_timer_bank1->ctl.irq_status);

    hw_icu->peri_clk_pwd.timer_26m = 1;
    hw_icu->peri_clk_pwd.timer_32k = 1;
}

static void timer_init(void) {
    hw_timer_bank0->ctl.v = 0;
    hw_timer_bank1->ctl.v = 0;

    hw_icu->peri_clk_pwd.timer_26m = 0;
    hw_icu->peri_clk_pwd.timer_32k = 0;

    intc_register_irq_handler(IRQ_SOURCE_TIMER, timer_isr);
    intc_enable_irq_source(IRQ_SOURCE_TIMER);
}

INIT_AT(timer_init, 02);

int timer_create(uint32_t count, timer_alarm_handler_t *func, bool once) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    int timer_num = find_free_timer(0);
    if (timer_num < 0) {
        GLOBAL_INT_RESTORE();
        return -1;
    }

    volatile hw_timer_bank_t *bank              = get_timer_bank_by_index(timer_num);
    int                       timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    timers_handlers[timer_num].type    = once ? TYPE_ONCE : TYPE_PERIODIC;
    timers_handlers[timer_num].handler = func;

    bank->counter[timer_num_in_bank] = count;
    timer_clear_irq_status(bank, 1 << timer_num_in_bank);

    GLOBAL_INT_RESTORE();
    return timer_num;
}

int timer_create_by_freq(uint32_t freq, timer_alarm_handler_t *func, bool once) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    int timer_num = find_free_timer(freq);
    if (timer_num < 0) {
        GLOBAL_INT_RESTORE();
        return -1;
    }

    volatile hw_timer_bank_t *bank              = get_timer_bank_by_index(timer_num);
    int                       timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    timers_handlers[timer_num].type    = once ? TYPE_ONCE : TYPE_PERIODIC;
    timers_handlers[timer_num].handler = func;

    bank->counter[timer_num_in_bank] = get_timer_frequency(timer_num) / freq;
    timer_clear_irq_status(bank, 1 << timer_num_in_bank);

    GLOBAL_INT_RESTORE();
    return timer_num;
}

void timer_start(int timer_num) {
    assert_timer_number(timer_num);
    assert(timers_handlers[timer_num].type != TYPE_NONE);

    hw_timer_bank_t *bank              = get_timer_bank_by_index(timer_num);
    int              timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    timer_clear_irq_status(bank, 1 << timer_num_in_bank);
    timer_bank_update_enable(bank, 1u << timer_num_in_bank, 0);
}

void timer_remove(int timer_num) {
    assert_timer_number(timer_num);

    hw_timer_bank_t *bank              = get_timer_bank_by_index(timer_num);
    int              timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);
    timer_bank_update_enable(bank, 0, 1u << timer_num_in_bank);

    timers_handlers[timer_num].handler = NULL;
    timers_handlers[timer_num].type    = TYPE_NONE;
}

uint32_t timer_pause_all(void) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    uint32_t enabled0 = hw_timer_bank0->ctl.enable;
    uint32_t enabled1 = hw_timer_bank1->ctl.enable;

    timer_bank_update_enable(hw_timer_bank0, 0, enabled0);
    timer_bank_update_enable(hw_timer_bank1, 0, enabled1);

    GLOBAL_INT_RESTORE();
    return enabled0 | (enabled1 << TIMERS_IN_BANK);
}

void timer_resume_all(uint32_t paused_mask) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    timer_bank_update_enable(hw_timer_bank0, paused_mask & ((1u << TIMERS_IN_BANK) - 1), 0);
    timer_bank_update_enable(hw_timer_bank1, (paused_mask >> TIMERS_IN_BANK) & ((1u << TIMERS_IN_BANK) - 1), 0);

    GLOBAL_INT_RESTORE();
}

