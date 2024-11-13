#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include "platform/stdio.h"
#include "utils/panic.h"


#define count_of(x)     (sizeof(x) / sizeof(x[0]))


/*-----------------------------------------------------------*/

void vTaskShell(__unused void *pvParams);

static StaticTask_t shellTaskTCB;
static StackType_t shellTaskStack[configMINIMAL_STACK_SIZE * 4];

/*-----------------------------------------------------------*/
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

    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}

