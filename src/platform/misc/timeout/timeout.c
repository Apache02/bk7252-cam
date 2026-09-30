#include "platform/timeout.h"
#include "hardware/timer.h"
#include "platform/cpu.h"
#include <stddef.h>

#define TIMEOUT_TICK_HZ 1000


static struct {
    struct timeout_t *head;
    int               timer;
} timeouts = {NULL, -1};

static void timeout_isr(int timer_num) {
    if (timeouts.head == NULL) {
        timer_remove(timer_num);
        timeouts.timer = -1;
        return;
    }
    if (timer_num != timeouts.timer) return;

    struct timeout_t *prev = NULL;
    for (struct timeout_t *current = timeouts.head; current; current = current->next) {
        current->ticks--;
        if (current->ticks == 0) {
            // finish timeout
            if (prev) prev->next = current->next;
            else timeouts.head = current->next;
        } else {
            prev = current;
        }
    }
}

bool create_timeout(struct timeout_t *t, uint32_t ms) {
    ms = ms > 0 ? ms : 1;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    if (timeouts.timer < 0) {
        int timer_num = timer_create_by_freq(TIMEOUT_TICK_HZ, &timeout_isr, false);
        if (timer_num < 0) {
            GLOBAL_INT_RESTORE();
            return false;
        }
        timeouts.timer = timer_num;
        timer_start(timer_num);
        // A fresh timer delivers its first tick a full period from now.
        t->ticks = ms;
    } else {
        // A running timer's next tick can land at any moment, so add one to never undershoot.
        t->ticks = ms + 1;
    }

    // add timeout to tail
    t->next = NULL;
    if (timeouts.head == NULL) {
        timeouts.head = t;
    } else {
        struct timeout_t *current = timeouts.head;
        while (current->next) current = current->next;
        current->next = t;
    }
    GLOBAL_INT_RESTORE();

    return true;
}

void finish_timeout(struct timeout_t *t) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    t->ticks = 0;
    struct timeout_t *prev = NULL;
    for (struct timeout_t *current = timeouts.head; current; current = current->next) {
        if (current == t) {
            if (prev) prev->next = current->next;
            else timeouts.head = current->next;
            break;
        }
        prev = current;
    }
    GLOBAL_INT_RESTORE();
}

bool is_timeout_finished(struct timeout_t *t) { return t->ticks == 0; }
