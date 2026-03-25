#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include "platform/stdio.h"
#include "utils/panic.h"
#include "hardware/wdt.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))


/*-----------------------------------------------------------*/

extern void vTaskShell(void *pvParams);

static StaticTask_t shellTaskTCB;
static StackType_t shellTaskStack[configMINIMAL_STACK_SIZE * 6];

/*-----------------------------------------------------------*/

static StaticTask_t wdtTaskTCB;
static StackType_t wdtTaskStack[40];

void vTaskWdt(__unused void *pvParams) {
    wdt_init();
    wdt_set(2000);
    wdt_up();

    for (;;) {
        wdt_ping();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*-----------------------------------------------------------*/


int main() {
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

    xTaskCreateStatic(
        vTaskWdt,
        "watchdog",
        count_of(wdtTaskStack),
        NULL,
        configMAX_PRIORITIES - 1,
        wdtTaskStack,
        &(wdtTaskTCB)
    );

    vPortHeapResetState();
    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}

