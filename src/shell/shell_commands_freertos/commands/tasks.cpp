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

// requires configUSE_TRACE_FACILITY
int command_tasks(int argc, const char *argv[]) {
#if configUSE_TRACE_FACILITY == 1
    uint32_t uxArraySize = uxTaskGetNumberOfTasks();
    size_t xTasksBufferSize = sizeof(TaskStatus_t) * (uxArraySize + 5);
    auto *pxTasksBuffer = static_cast<TaskStatus_t *>(pvPortMalloc(xTasksBufferSize));

    if (!pxTasksBuffer) {
        printf(COLOR_RED("Can't allocate %u bytes") "\r\n", (unsigned) xTasksBufferSize);
        return 1;
    }

    uint32_t ulTotalRunTime = 0;
    uxArraySize = uxTaskGetSystemState(pxTasksBuffer, uxArraySize + 5, &ulTotalRunTime);

    if (uxArraySize == 0) {
        printf(COLOR_RED("uxTaskGetSystemState failed") "\r\n");
        vPortFree(pxTasksBuffer);
        return 1;
    }

    printf("| %16s | %10s | %8s | %10s | %7s | %7s | %7s |\r\n",
           "name", "state", "priority", "addr", "free", "total", "cpu%");
    printf("| %16s | %10s | %8s | %10s | %7s | %7s | %7s |\r\n",
           "----------------", "----------", "--------", "----------", "-------", "-------", "-------");

    for (unsigned int i = 0; i < uxArraySize; i++) {
        const auto &t = pxTasksBuffer[i];

        const char *stateLabel = get_task_state_label(t.eCurrentState);

        uint32_t freeWords  = t.usStackHighWaterMark;
        uint32_t freeBytes  = freeWords * sizeof(StackType_t);
        char stackTotalBuf[12];
#if configRECORD_STACK_HIGH_ADDRESS == 1
        uint32_t totalWords = static_cast<uint32_t>(t.pxTopOfStack - t.pxStackBase) + 1;
        uint32_t totalBytes = totalWords * sizeof(StackType_t);
        snprintf(stackTotalBuf, sizeof(stackTotalBuf), "%6lu", totalBytes);
#else
        snprintf(stackTotalBuf, sizeof(stackTotalBuf), "n/a");
#endif

        char cpuBuf[16];
        const char *cpuPtr = "  n/a";
        if (ulTotalRunTime > 0) {
            int permille = pxTasksBuffer[i].ulRunTimeCounter * 1000UL / ulTotalRunTime;
            snprintf(cpuBuf, sizeof(cpuBuf), "%3d.%d%%", permille / 10, permille % 10);
            cpuPtr = cpuBuf;
        }

        printf(
            "| %16s | %10s | %8ld | 0x%08lx | %7ld | %7s | %7s |\r\n",
            t.pcTaskName,
            stateLabel,
            t.uxCurrentPriority,
            reinterpret_cast<unsigned long>(t.pxStackBase),
            freeBytes,
            stackTotalBuf,
            cpuPtr
        );
    }

    vPortFree(pxTasksBuffer);
#else
    printf("required configUSE_TRACE_FACILITY == 1\r\n");
#endif

    return 0;
}
