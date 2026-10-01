#include "platform/timeout.h"
#include "hardware/timer.h"
#include "platform/cpu.h"
#include <stddef.h>

#define TIMEOUT_TICK_HZ    1000
#define REMOVE_DELAY_TICKS 1000
#define MS_TO_TICKS(ms)    (ms)


static struct {
    struct timeout_t *head;
    int               timer;
    int               remove_delay;
} timeouts = {NULL, -1, REMOVE_DELAY_TICKS};


#define REMOVE_CURRENT(head, prev, current) \
    if (prev) {                             \
        prev->next = current->next;         \
    } else {                                \
        head = current->next;               \
    }


static void timeout_isr(int timer_num) {
    if (timeouts.head == NULL) {
        if (timeouts.remove_delay <= 0) {
            timer_remove(timer_num);
            timeouts.timer = -1;
        } else {
            timeouts.remove_delay--;
        }
        return;
    }
    if (timer_num != timeouts.timer) return;

    struct timeout_t *prev = NULL;
    for (struct timeout_t *current = timeouts.head; current; current = current->next) {
        if (current->ticks > 0) {
            current->ticks--;
        }
        if (current->ticks == 0) {
            // finish timeout
            REMOVE_CURRENT(timeouts.head, prev, current);
            timeouts.remove_delay = REMOVE_DELAY_TICKS;
        } else {
            prev = current;
        }
    }
}

bool create_timeout(struct timeout_t *t, const uint32_t ms) {
    const bool     infinite_timeout = ms == INFINITE_TIMEOUT;
    const uint32_t ticks            = infinite_timeout ? 0xffffffff : (MS_TO_TICKS(ms) + 1);
    if (ticks == 0) return false;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    t->ticks = ticks;
    t->next  = NULL;

    if (!infinite_timeout) {
        if (timeouts.timer < 0) {
            const int timer_num = timer_create_by_freq(TIMEOUT_TICK_HZ, &timeout_isr, false);
            if (timer_num < 0) {
                GLOBAL_INT_RESTORE();
                return false;
            }
            timeouts.timer = timer_num;
            timer_start(timer_num);
        }

        // add timeout to tail
        if (timeouts.head == NULL) {
            timeouts.head = t;
        } else {
            struct timeout_t *current = timeouts.head;
            while (current->next) current = current->next;
            current->next = t;
        }
    }

    GLOBAL_INT_RESTORE();

    return true;
}

void free_timeout(struct timeout_t *t) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    t->ticks               = 0;
    struct timeout_t *prev = NULL;
    for (struct timeout_t *current = timeouts.head; current; current = current->next) {
        if (current == t) {
            REMOVE_CURRENT(timeouts.head, prev, current);
            timeouts.remove_delay = REMOVE_DELAY_TICKS;
            break;
        }
        prev = current;
    }
    GLOBAL_INT_RESTORE();
}

bool is_timeout_finished(const struct timeout_t *t) { return t->ticks == 0; }
