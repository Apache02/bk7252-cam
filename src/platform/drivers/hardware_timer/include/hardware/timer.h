#ifndef _HARDWARE_TIMER_H
#define _HARDWARE_TIMER_H

#include <stdint.h>
#include <stdbool.h>


typedef void (timer_alarm_handler_t)(int timer_num);


#ifdef __cplusplus
extern "C" {
#endif

void timer_init();

int timer_create(bool once, uint32_t count, int divider, timer_alarm_handler_t *func);

void timer_remove(int timer_num);

uint64_t time_us_64();

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_TIMER_H
