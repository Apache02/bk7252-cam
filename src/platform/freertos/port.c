#include <FreeRTOS.h>
#include <task.h>
#include "hardware/timer.h"
#include "hardware/cpu.h"

#include <sys/cdefs.h>


uint32_t ulCriticalNesting;


void vPortEnableInterrupts() {
    if (portIsInFIQ()) return;
    if (portIsInIRQ()) {
        portENABLE_FIQ();
        return;
    }

    portENABLE_IRQ();
    portENABLE_FIQ();
}

void vPortDisableInterrupts() {
    if (portIsInFIQ()) return;
    if (portIsInIRQ()) {
        portDISABLE_FIQ();
        return;
    }

    portDISABLE_FIQ();
    portDISABLE_IRQ();
}

void vPortEnterCritical(void) {
    vPortDisableInterrupts();

    ulCriticalNesting++;
}

void vPortExitCritical(void) {
    if (ulCriticalNesting > 0) {
        ulCriticalNesting--;

        if (ulCriticalNesting == 0) {
            vPortEnableInterrupts();
        }
    }
}

static void xPortSysTickHandler(__unused int timer_num) {
    if (xTaskIncrementTick() != pdFALSE) {
        /* Select a new task to run. */
        vTaskSwitchContext();
    }
}

extern void vPortStartFirstTask(void);

BaseType_t xPortStartScheduler(void) {
    ulCriticalNesting = 0;

    /* Start the timer that generates the tick ISR. Interrupts are disabled here already. */
    int timer = timer_create(26000000 / configTICK_RATE_HZ, xPortSysTickHandler, false);
    if (timer < 0) {
        return 0;
    }

    timer_start(timer);

    vPortStartFirstTask();

    return 0;
}

void vPortEndScheduler(void) {
    // TODO: clear timer
}

/*-----------------------------------------------------------*/
/*
 * Initialize the stack of a task to look exactly as if a call to
 * portSAVE_CONTEXT had been called.
 *
 * See header file for description.
 */
StackType_t *pxPortInitialiseStack(StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters) {
    const StackType_t *pxOriginalTOS = pxTopOfStack;

    /* To ensure asserts in tasks.c don't fail, although in this case the assert
    is not really required. */
    pxTopOfStack--;

    /* Setup the initial stack of the task.  The stack is set exactly as
    expected by the portRESTORE_CONTEXT() macro. */

    /* First on the stack is the return address - which in this case is the
    start of the task.  The offset is added to make the return address appear
    as it would within an IRQ ISR. */
    *pxTopOfStack = (StackType_t) pxCode + portINSTRUCTION_SIZE;
    pxTopOfStack--;

    *pxTopOfStack = (StackType_t) 0xaaaaaaaa;    /* R14 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) pxOriginalTOS; /* Stack used when task starts goes in R13. */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x12121212;    /* R12 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x11111111;    /* R11 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x10101010;    /* R10 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x09090909;    /* R9 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x08080808;    /* R8 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x07070707;    /* R7 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x06060606;    /* R6 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x05050505;    /* R5 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x04040404;    /* R4 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x03030303;    /* R3 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x02020202;    /* R2 */
    pxTopOfStack--;
    *pxTopOfStack = (StackType_t) 0x01010101;    /* R1 */
    pxTopOfStack--;

    /* When the task starts is will expect to find the function parameter in
    R0. */
    *pxTopOfStack = (StackType_t) pvParameters; /* R0 */
    pxTopOfStack--;

    /* The status register is set for system mode, with interrupts enabled. */
    *pxTopOfStack = ((uint32_t) pxCode & 0x01)
                    ? (StackType_t) portINITIAL_SPSR | portTHUMB_MODE_BIT
                    : (StackType_t) portINITIAL_SPSR;
    pxTopOfStack--;

    /* Interrupt flags cannot always be stored on the stack and will
    instead be stored in a variable, which is then saved as part of the
    tasks context. */
    *pxTopOfStack = 0;

    return pxTopOfStack;
}
