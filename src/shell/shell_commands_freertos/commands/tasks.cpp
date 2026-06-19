#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/console_colors.h"
#include "shell/Table.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

static const char *const TASK_STATE_LABEL_MAP[] = {
    [eTaskState::eRunning] = "running",     [eTaskState::eReady] = "ready",     [eTaskState::eBlocked] = "blocked",
    [eTaskState::eSuspended] = "suspended", [eTaskState::eDeleted] = "deleted", [eTaskState::eInvalid] = "invalid",
};

static const char *get_task_state_label(unsigned int state) {
    return state < count_of(TASK_STATE_LABEL_MAP) ? TASK_STATE_LABEL_MAP[state] : "?";
}

static const Table::ColumnDef table_def[] = {
    {"name", 16, "%s", Table::Align::Left},      {"state", 10, "%s", Table::Align::Right},
    {"priority", 8, "%ld", Table::Align::Right}, {"addr", 10, "0x%08lx", Table::Align::Right},
    {"size", 7, "%s", Table::Align::Right},      {"free", 7, "%7ld", Table::Align::Right},
    {"cpu%", 7, "%s", Table::Align::Right},
};

// requires configUSE_TRACE_FACILITY
int command_tasks(int argc, const char *argv[]) {
#if configUSE_TRACE_FACILITY == 1
    uint32_t uxArraySize      = uxTaskGetNumberOfTasks();
    size_t   xTasksBufferSize = sizeof(TaskStatus_t) * (uxArraySize + 5);
    auto    *pxTasksBuffer    = static_cast<TaskStatus_t *>(pvPortMalloc(xTasksBufferSize));

    if (!pxTasksBuffer) {
        printf(COLOR_RED("Can't allocate %u bytes") "\r\n", (unsigned)xTasksBufferSize);
        return 1;
    }

    uint32_t ulTotalRunTime = 0;
    uxArraySize             = uxTaskGetSystemState(pxTasksBuffer, uxArraySize + 5, &ulTotalRunTime);

    if (uxArraySize == 0) {
        printf(COLOR_RED("uxTaskGetSystemState failed") "\r\n");
        vPortFree(pxTasksBuffer);
        return 1;
    }

    auto *table = new Table(table_def, count_of(table_def));
    table->printHeader();

    for (unsigned int i = 0; i < uxArraySize; i++) {
        const auto &t = pxTasksBuffer[i];

        const char *stateLabel = get_task_state_label(t.eCurrentState);

        uint32_t freeWords = t.usStackHighWaterMark;
        uint32_t freeBytes = freeWords * sizeof(StackType_t);
        char     stackTotalBuf[12];
#if configRECORD_STACK_HIGH_ADDRESS == 1
        uint32_t totalWords = static_cast<uint32_t>(t.pxTopOfStack - t.pxStackBase) + 1;
        uint32_t totalBytes = totalWords * sizeof(StackType_t);
        snprintf(stackTotalBuf, sizeof(stackTotalBuf), "%6lu", totalBytes);
#else
        snprintf(stackTotalBuf, sizeof(stackTotalBuf), "n/a");
#endif

        char        cpuBuf[16];
        const char *cpuPtr = "  n/a";
        if (ulTotalRunTime > 0) {
            int permille = pxTasksBuffer[i].ulRunTimeCounter * 1000UL / ulTotalRunTime;
            snprintf(cpuBuf, sizeof(cpuBuf), "%3d.%d%%", permille / 10, permille % 10);
            cpuPtr = cpuBuf;
        }

        auto *row = table->createRow();
        row->set("name", t.pcTaskName);
        row->set("state", stateLabel);
        row->set("priority", t.uxCurrentPriority);
        row->set("addr", t.pxStackBase);
        row->set("size", stackTotalBuf);
        row->set("free", freeBytes);
        row->set("cpu%", cpuPtr);

        table->printRow(row);

        delete row;
    }

    delete table;

    printf("\r\n");

    vPortFree(pxTasksBuffer);
#else
    printf("required configUSE_TRACE_FACILITY == 1\r\n");
#endif

    return 0;
}
