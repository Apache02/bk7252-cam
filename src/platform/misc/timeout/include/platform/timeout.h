#ifndef _PLATFORM_TIMEOUT_H
#define _PLATFORM_TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>


struct timeout_t {
    struct timeout_t *next;
    volatile uint32_t ticks;
};


#ifdef __cplusplus
extern "C" {
#endif

bool create_timeout(struct timeout_t *t, uint32_t ms);

void finish_timeout(struct timeout_t *t);

bool is_timeout_finished(struct timeout_t *t);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_TIMEOUT_H
