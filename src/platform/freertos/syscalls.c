#include "platform/sched.h"
#include "platform/unistd.h"
#include <FreeRTOS.h>
#include <task.h>

// Strong override of port_newlib/syscalls.c's weak default: under FreeRTOS, yielding
// means a context switch (portYIELD()), not WFI() — another task may be ready to run.
int sched_yield(void) {
    portYIELD();
    return 0;
}

// Strong override of port_newlib/syscalls.c's weak default: the task blocks instead of
// waiting on a hardware timer. The wait is rounded up to whole milliseconds and then to a tick.
int usleep(useconds_t us) {
    if (us == 0) return 0;

    const TickType_t ms = (us + 999) / 1000;
    vTaskDelay((ms * configTICK_RATE_HZ + 999) / 1000);
    return 0;
}
