#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include "platform/stdio.h"
#include "utils/panic.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))


/*-----------------------------------------------------------*/

void vTaskShell(__unused void *pvParams);

static StaticTask_t shellTaskTCB;
static StackType_t shellTaskStack[configMINIMAL_STACK_SIZE * 6];

/*-----------------------------------------------------------*/

PRIVILEGED_DATA static uint8_t ucHeap[configMAIN_HEAP_SIZE];

void init_heap(void) {
    static const HeapRegion_t xHeapRegions[] =
            {
                    {ucHeap,                 sizeof(ucHeap)},
                    {(uint8_t *) 0x00900000, configADDITIONAL_HEAP_SIZE},
                    {NULL,                   0}
            };

    vPortHeapResetState();
    vPortDefineHeapRegions(xHeapRegions);
}

/*-----------------------------------------------------------*/


int main(void) {
    platform_stdio_init();
    setvbuf(stdout, NULL, _IONBF, 0);

    xTaskCreateStatic(
            vTaskShell,
            "shell",
            count_of(shellTaskStack),
            NULL,
            configMAX_PRIORITIES - 1,
            shellTaskStack,
            &(shellTaskTCB)
    );

    init_heap();
    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}

