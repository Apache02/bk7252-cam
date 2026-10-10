#ifndef _PLATFORM_TIMEOUT_H
#define _PLATFORM_TIMEOUT_H

#include <stdint.h>
#include <stdbool.h>


// Example: wait for a flag for at most 10 ms.
//
//   struct timeout_t deadline;
//   if (!create_timeout(&deadline, 10)) return -ENOMEM;
//
//   while (!done) {
//       if (is_timeout_finished(&deadline)) return -ETIMEDOUT; // already off the list
//   }
//   free_timeout(&deadline); // left early, so cancel it


struct timeout_t {
    struct timeout_t *next;
    volatile uint32_t ticks;
};


// A timeout that never expires. It needs no hardware timer, so it also works with
// CPU interrupts disabled.
#define INFINITE_TIMEOUT (0)


#ifdef __cplusplus
extern "C" {
#endif

// Starts a timeout of at least `ms` milliseconds. The struct must stay valid until
// the timeout has finished or been freed. Returns false if no hardware timer is free or `ms` does
// not fit in timer ticks. Expiry is counted in the timer interrupt, so it never
// happens while CPU interrupts are disabled.
bool create_timeout(struct timeout_t *t, uint32_t ms);

// Cancels the timeout. Call it when leaving before the timeout finishes.
void free_timeout(struct timeout_t *t);

// A finished timeout is already off the list, so its struct may go out of scope
// without free_timeout().
bool is_timeout_finished(const struct timeout_t *t);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_TIMEOUT_H
