#include "platform/sched.h"
#include <FreeRTOS.h>
#include <task.h>

// Strong override of port_newlib/syscalls.c's weak default: under FreeRTOS, yielding
// means a context switch (portYIELD()), not WFI() — another task may be ready to run.
int sched_yield(void) {
    portYIELD();
    return 0;
}
