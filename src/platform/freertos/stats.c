#include <stdint.h>
#include "hardware/time.h"
#include <FreeRTOS.h>

// activate:
// configGENERATE_RUN_TIME_STATS = 1
// configUSE_STATS_FORMATTING_FUNCTIONS = 1

static uint32_t accumulator = 0;
static uint32_t previous = 0;
static uint32_t remainder = 0;


void vConfigureTimerForRunTimeStats() {
    accumulator = 0;
    previous = 0;
    ulGetRunTimeCounterValue();
}

uint32_t ulGetRunTimeCounterValue() {
#if ( configGENERATE_RUN_TIME_STATS == 1 )
    uint32_t current = get_us_counter();
    uint32_t delta_us = current - previous;
    previous = current;

    remainder += delta_us;
    uint32_t ticks = remainder / 128;
    remainder %= 128;
    accumulator += ticks;
#endif

    return accumulator;
}
