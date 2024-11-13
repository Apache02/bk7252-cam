#ifndef _HARDWARE_TIMER_H
#define _HARDWARE_TIMER_H

#include <stdint.h>
#include <stdbool.h>


typedef void (timer_alarm_handler_t)(int timer_num);


#ifdef __cplusplus
extern "C" {
#endif

void timer_init();

int timer_create(uint32_t count, timer_alarm_handler_t *func, bool once);

int timer_create_by_freq(uint32_t freq, timer_alarm_handler_t *func, bool once);

void timer_start(int timer_num);

void timer_remove(int timer_num);

int timer_read(int timer_num);

uint64_t time();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_TIMER_H
