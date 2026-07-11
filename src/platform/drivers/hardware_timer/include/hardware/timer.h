#ifndef _HARDWARE_TIMER_H
#define _HARDWARE_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef void(timer_alarm_handler_t)(int timer_num);

#ifdef __cplusplus
extern "C" {
#endif

int timer_create(uint32_t count, timer_alarm_handler_t *func, bool once);

int timer_create_by_freq(uint32_t freq, timer_alarm_handler_t *func, bool once);

void timer_start(int timer_num);

void timer_remove(int timer_num);

void timer_reset(void);

// Disables every currently-running timer, returning a bitmask (bit i =
// timer i) to pass to timer_resume_all() afterward.
uint32_t timer_pause_all(void);

// Re-enables timers previously paused by timer_pause_all().
void timer_resume_all(uint32_t paused_mask);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_TIMER_H
