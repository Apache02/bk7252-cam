#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/console_colors.h"


#undef count_of
#define count_of(x)     (sizeof(x) / sizeof(x[0]))

static const char *const TASK_STATE_LABEL_MAP[] = {
    [eTaskState::eRunning] = "running",
    [eTaskState::eReady] = "ready",
    [eTaskState::eBlocked] = "blocked",
    [eTaskState::eSuspended] = "suspended",
    [eTaskState::eDeleted] = "deleted",
    [eTaskState::eInvalid] = "invalid",
};

static const char *get_task_state_label(unsigned int state) {
    return state < count_of(TASK_STATE_LABEL_MAP)
               ? TASK_STATE_LABEL_MAP[state]
               : "?";
}

const StackType_t *xCalcHighWaterMark(const StackType_t *pxStack) {
    while (*pxStack == 0xa5a5a5a5) {
        pxStack++;
    }
    return pxStack;
}


// requires configUSE_TRACE_FACILITY
int command_tasks(int argc, const char *argv[]) {
#if configUSE_TRACE_FACILITY == 1
    uint32_t uxArraySize = uxTaskGetNumberOfTasks();
    size_t xTasksBufferSize = (sizeof(TaskStatus_t) * (uxArraySize + 5));
    auto *pxTasksBuffer = static_cast<TaskStatus_t *>(pvPortMalloc(xTasksBufferSize));

    if (!pxTasksBuffer) {
        printf(COLOR_RED("Can't allocate %d bytes") "\r\n", xTasksBufferSize);
        return 1;
    }

    uxArraySize = uxTaskGetSystemState(pxTasksBuffer, uxArraySize, nullptr);

    if (uxArraySize > 0) {
        printf("| %16s | %10s | %8s | %10s | %10s |\r\n",
               "name", "state", "priority", "stack", "free"
        );
        printf("| %16s | %10s | %8s | %10s | %10s |\r\n",
               "----------------", "----------", "--------", "----------", "----------"
        );

        for (unsigned int i = 0; i < uxArraySize; i++) {
            auto state = pxTasksBuffer[i].eCurrentState;
            const char *stateLabel = get_task_state_label(state);
            const StackType_t *pxHWM = xCalcHighWaterMark(pxTasksBuffer[i].pxStackBase);

            printf(
                "| %16s | %10s | %8ld | 0x%8p | %10ld |\r\n",
                pxTasksBuffer[i].pcTaskName,
                stateLabel,
                pxTasksBuffer[i].uxCurrentPriority,
                static_cast<void *>(pxTasksBuffer[i].pxStackBase),
                static_cast<unsigned long>(pxHWM - pxTasksBuffer[i].pxStackBase)
            );
        }
    }

    vPortFree(pxTasksBuffer);
#else
    printf("required configUSE_TRACE_FACILITY == 1\r\n");
#endif

    return 0;
}
