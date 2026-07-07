// newlib's <sched.h> guards sched_yield()'s declaration behind _POSIX_THREADS /
// _POSIX_PRIORITY_SCHEDULING; neither is defined project-wide, so define the narrower
// one here, scoped to this translation unit, just to unlock the declaration.
#define _POSIX_PRIORITY_SCHEDULING
#include <sched.h>

#include <FreeRTOS.h>
#include <task.h>

// Strong override of port_newlib/syscalls.c's weak default: under FreeRTOS, yielding
// means a context switch (portYIELD()), not WFI() — another task may be ready to run.
int sched_yield(void) {
    portYIELD();
    return 0;
}
