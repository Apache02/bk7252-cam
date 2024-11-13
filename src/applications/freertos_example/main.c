#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <timers.h>
#include <semphr.h>

#include <stdio.h>

#include "hardware/intc.h"
#include "hardware/timer.h"
#include "platform/stdio.h"
#include "utils/panic.h"

#include "utils/sleep.h"


/*-----------------------------------------------------------*/

static void vStaticTask1(__unused void *parameters) {
    for (int i = 0;; i++) {
        portENTER_CRITICAL();
        printf("%s #%d\r\n", __PRETTY_FUNCTION__, i);
        usleep(500000);
        portEXIT_CRITICAL();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static StaticTask_t exampleTaskTCB;
static StackType_t exampleTaskStack[configMINIMAL_STACK_SIZE];

/*-----------------------------------------------------------*/

static void vDynamicTask2(__unused void *parameters) {
    for (int i = 0;; i++) {
        printf("%s #%d\r\n", __PRETTY_FUNCTION__, i);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*-----------------------------------------------------------*/


static void init_hardware() {
    intc_init();
    platform_stdio_init();
    timer_init();
}

static void init_software() {
}

int main(void) {
    init_hardware();
    init_software();

    printf("Example FreeRTOS Project\r\n");
    usleep(1000);

    xTaskCreateStatic(
            vStaticTask1,
            "static1",
            sizeof(exampleTaskStack) / sizeof(exampleTaskStack[0]),
            NULL,
            configMAX_PRIORITIES - 1,
            &(exampleTaskStack[0]),
            &(exampleTaskTCB)
    );

    xTaskCreate(
            vDynamicTask2,
            "dynamic2",
            configMINIMAL_STACK_SIZE,
            NULL,
            configMAX_PRIORITIES - 1,
            NULL
    );

    /* Start the scheduler. */
    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}

/*-----------------------------------------------------------*/

struct tcb_t {
    volatile StackType_t *pxTopOfStack;
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    UBaseType_t uxPriority;
    StackType_t *pxStack;
    char pcTaskName[configMAX_TASK_NAME_LEN];
};

void vSwitchHook(void *pvParam) {
//    struct tcb_t *pxTCB = pvParam;
//    printf("switch to %s @ 0x%08x (stack: 0x%08x)\r\n", pxTCB->pcTaskName, pxTCB, &pxTCB->pxTopOfStack[0]);
//
//    printf("CriticalNesting: %lu\r\n", pxTCB->pxTopOfStack[0]);
//    printf("SPSR: 0x%08lx\r\n", pxTCB->pxTopOfStack[1]);
//    printf("R0: 0x%08lx\r\n", pxTCB->pxTopOfStack[2]);
//    printf("R1: 0x%08lx\r\n", pxTCB->pxTopOfStack[3]);
//    printf("R2: 0x%08lx\r\n", pxTCB->pxTopOfStack[4]);
//    printf("R3: 0x%08lx\r\n", pxTCB->pxTopOfStack[5]);
//    printf("R4: 0x%08lx\r\n", pxTCB->pxTopOfStack[6]);
//    printf("R5: 0x%08lx\r\n", pxTCB->pxTopOfStack[7]);
//    printf("R6: 0x%08lx\r\n", pxTCB->pxTopOfStack[8]);
//    printf("R7: 0x%08lx\r\n", pxTCB->pxTopOfStack[9]);
//    printf("R8: 0x%08lx\r\n", pxTCB->pxTopOfStack[10]);
//    printf("R9: 0x%08lx\r\n", pxTCB->pxTopOfStack[11]);
//    printf("R10: 0x%08lx\r\n", pxTCB->pxTopOfStack[12]);
//    printf("R11: 0x%08lx\r\n", pxTCB->pxTopOfStack[13]);
//    printf("R12: 0x%08lx\r\n", pxTCB->pxTopOfStack[14]);
//    printf("R13 (SP): 0x%08lx\r\n", pxTCB->pxTopOfStack[15]);
//    printf("R14 (LR): 0x%08lx\r\n", pxTCB->pxTopOfStack[16]);
//    printf("task entry: 0x%08lx\r\n", pxTCB->pxTopOfStack[17]);
//    printf("?: 0x%08lx\r\n", pxTCB->pxTopOfStack[18]);
//    printf("---------------\r\n");
}
