#include "hardware/timer.h"
#include "timer_regs.h"
#include "sys_counter.h"
#include "hardware/icu.h"
#include "hardware/intc.h"
#include "hardware/cpu.h"
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


#define assert_timer_number(i)       assert_true((i >= 0) && (i < TIMERS_TOTAL), "Invalid timer number")


static void timer_irq_handler() {
    uint32_t status0 = hw_timer_bank0->ctl.irq_status;
    uint32_t status1 = hw_timer_bank1->ctl.irq_status;
    uint32_t status_all = status0 | (status1 << TIMERS_IN_BANK);

    for (int i = 0; i < TIMERS_TOTAL; i++) {
        if (!(status_all & (1 << i))) continue;

        timer_alarm_handler_t *handler_func = timers_handlers[i].handler;

        if (handler_func) {
            handler_func(i);
        }

        if (timers_handlers[i].type == TYPE_ONCE) {
            // disable hw timer
            volatile hw_timer_bank_t *bank = get_timer_bank_by_index(i);
            int timer_num_in_bank = get_timer_num_in_bank_by_index(i);
            bank->ctl.enable &= ~(1 << timer_num_in_bank);

            timers_handlers[i].handler = NULL;
            timers_handlers[i].type = TYPE_NONE;
        }
    }

    // clear timers
    if (status0) {
        do {
            hw_timer_bank0->ctl.irq_status = status0;
        } while (hw_timer_bank0->ctl.irq_status & status0);
    }
    if (status1) {
        do {
            hw_timer_bank1->ctl.irq_status = status1;
        } while (hw_timer_bank1->ctl.irq_status & status1);
    }
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

static void register_sys_timer() {
    int timer_num = find_free_timer(1);
    const int timer_clock_freq = get_timer_frequency(timer_num);
    const int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);
    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);

    timers_handlers[timer_num].type = TYPE_PERIODIC;
    timers_handlers[timer_num].handler = &sys_counter_tick;

    bank->counter[timer_num_in_bank] = timer_clock_freq;
    bank->ctl.irq_status &= ~(1 << timer_num_in_bank); // start after 1 second
    bank->ctl.enable |= (1 << timer_num_in_bank);
}

void timer_init() {
    hw_timer_bank0->ctl.enable = 0;
    hw_timer_bank0->ctl.irq_status = 0;
    hw_timer_bank0->ctl.clk_divider = 0;

    hw_timer_bank1->ctl.enable = 0;
    hw_timer_bank1->ctl.irq_status = 0;
    hw_timer_bank1->ctl.clk_divider = 0;

    hw_icu->peri_clk_pwd.timer_26m = 0;
    hw_icu->peri_clk_pwd.timer_32k = 0;

    register_sys_timer();

    intc_register_irq_handler(IRQ_SOURCE_TIMER, timer_irq_handler);
    intc_enable_irq_source(IRQ_SOURCE_TIMER);
}

int timer_create(uint32_t count, timer_alarm_handler_t *func, bool once) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    int timer_num = find_free_timer(0);
    if (timer_num < 0) {
        GLOBAL_INT_RESTORE();
        return -1;
    }

    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);
    int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    timers_handlers[timer_num].type = once ? TYPE_ONCE : TYPE_PERIODIC;
    timers_handlers[timer_num].handler = func;

    bank->counter[timer_num_in_bank] = count;
    bank->ctl.irq_status &= ~(1 << timer_num_in_bank);

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

    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);
    int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    timers_handlers[timer_num].type = once ? TYPE_ONCE : TYPE_PERIODIC;
    timers_handlers[timer_num].handler = func;

    bank->counter[timer_num_in_bank] = get_timer_frequency(timer_num) / freq;
    bank->ctl.irq_status &= ~(1 << timer_num_in_bank);

    GLOBAL_INT_RESTORE();
    return timer_num;
}

void timer_start(int timer_num) {
    assert_timer_number(timer_num);
    assert_true(timers_handlers[timer_num].type != TYPE_NONE, "Timer is empty");

    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);
    int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);

    bank->ctl.enable |= (1 << timer_num_in_bank);
}

void timer_remove(int timer_num) {
    assert_timer_number(timer_num);

    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);
    int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);
    bank->ctl.enable &= ~(1 << timer_num_in_bank);

    timers_handlers[timer_num].type = TYPE_NONE;
    timers_handlers[timer_num].handler = NULL;
}

// not working
int timer_read(int timer_num) {
    assert_timer_number(timer_num);

    volatile hw_timer_bank_t *bank = get_timer_bank_by_index(timer_num);
    int timer_num_in_bank = get_timer_num_in_bank_by_index(timer_num);
    bank->read_ctl.read_index = timer_num_in_bank;
    bank->read_ctl.read_op = 1;

    int timeout = 120000;
    while (bank->read_ctl.read_op) {
        if (--timeout <= 0) return -1;
    }

    return bank->read_value;
}
