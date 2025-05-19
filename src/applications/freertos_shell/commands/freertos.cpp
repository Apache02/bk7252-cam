#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/Console.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))

static const char *const TASK_STATE_LABEL_MAP[] = {
        [eTaskState::eRunning] = "running",
        [eTaskState::eReady] = "ready",
        [eTaskState::eBlocked] = "blocked",
        [eTaskState::eSuspended] = "suspended",
        [eTaskState::eDeleted] = "deleted",
        [eTaskState::eInvalid] = "invalid",
};

#define GET_TASK_STATE_LABEL(state)     (state >= 0 && count_of(TASK_STATE_LABEL_MAP)) \
    ? TASK_STATE_LABEL_MAP[state]                                                      \
    : "?"


const StackType_t *xCalcHighWaterMark(const StackType_t *pxStack) {
    while (*pxStack == 0xa5a5a5a5) {
        pxStack++;
    }
    return pxStack;
}

void command_tasks(__unused Console &c) {
    uint32_t uxArraySize = uxTaskGetNumberOfTasks();
    size_t xTasksBufferSize = (sizeof(TaskStatus_t) * uxArraySize);
    TaskStatus_t *pxTasksBuffer = static_cast<TaskStatus_t *>(pvPortMalloc(xTasksBufferSize));

    if (!pxTasksBuffer) {
        printf(COLOR_RED("Can't allocate %d bytes") "\r\n", xTasksBufferSize);
        return;
    }

    uxArraySize = uxTaskGetSystemState(pxTasksBuffer, uxArraySize, NULL);

    if (uxArraySize > 0) {
        printf("| %16s | %10s | %8s | %10s | %10s |\r\n",
               "name", "state", "priority", "stack", "free (w)"
        );
        printf("| %16s | %10s | %8s | %10s | %10s |\r\n",
               "----------------", "----------", "--------", "----------", "----------"
        );

        for (unsigned int i = 0; i < uxArraySize; i++) {
            auto state = pxTasksBuffer[i].eCurrentState;
            const char *stateLabel = GET_TASK_STATE_LABEL(state);
            const StackType_t *pxHWM = xCalcHighWaterMark(pxTasksBuffer[i].pxStackBase);

            printf(
                    "| %16s | %10s | %8ld | %10p | %10ld |\r\n",
                    pxTasksBuffer[i].pcTaskName,
                    stateLabel,
                    pxTasksBuffer[i].uxCurrentPriority,
                    static_cast<void *>(pxTasksBuffer[i].pxStackBase),
                    ((uint32_t) pxHWM - (uint32_t) pxTasksBuffer[i].pxStackBase) / sizeof(StackType_t)
            );
        }
    }

    vPortFree(pxTasksBuffer);
}

extern char end;
extern char _stack_unused;
extern "C" void *_sbrk(ptrdiff_t incr);

void command_free(__unused Console &c) {
    size_t xFreeHeapSizeCurrent = xPortGetFreeHeapSize();

    if (xFreeHeapSizeCurrent == 0) {
        vPortFree(pvPortMalloc(32));
        xFreeHeapSizeCurrent = xPortGetFreeHeapSize();
    }

    if (xFreeHeapSizeCurrent == 0) {
        printf("FreeRTOS head not available\r\n");
    } else {
        size_t xFreeHeapSizeMinimal = xPortGetMinimumEverFreeHeapSize();
        size_t xUsedHeapSizeCurrent = configTOTAL_HEAP_SIZE - xFreeHeapSizeCurrent;

        printf("== FreeRTOS heap ==\r\n");
        printf("           total: %d\r\n", configTOTAL_HEAP_SIZE);
        printf("            free: %d\r\n", xFreeHeapSizeCurrent);
        printf("            used: %d | %d%%\r\n", xUsedHeapSizeCurrent, xUsedHeapSizeCurrent * 100 / configTOTAL_HEAP_SIZE);
        printf("    minimum free: %d\r\n", xFreeHeapSizeMinimal);
        printf("\r\n");
    }


    size_t total = &_stack_unused - &end;
    size_t used = (char *) _sbrk(0) - &end;
    printf("== newlib heap ==\r\n");
    printf("           total: %d\r\n", total);
    printf("            free: %d\r\n", total - used);
    printf("            used: %d | %d%%\r\n", used, used * 100 / total);
    printf("\r\n");
}
