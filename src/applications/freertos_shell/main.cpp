#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include "platform/stdio.h"
#include "platform/panic.h"
#include "hardware/wdt.h"
#include "hardware/sctrl.h"

// #include "net.h"
#include "utils/busy_wait.h"
#include "shell_handlers.h"


#define count_of(x) (sizeof(x) / sizeof(x[0]))

/*-----------------------------------------------------------*/
// Watchdog task

static StaticTask_t wdtTaskTCB;
static StackType_t  wdtTaskStack[40];

void vTaskWdt(__unused void *pvParams) {
    wdt_init();
    wdt_down();
    wdt_set(2000);
    wdt_up();

    for (;;) {
        wdt_ping();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*-----------------------------------------------------------*/
// Shell task

static StaticTask_t shellTaskTCB;
static StackType_t  shellTaskStack[configMINIMAL_STACK_SIZE * 6];

static inline void print_welcome() {
    printf("\r\n");
    printf("Shell is ready");
    printf("\r\n\n");
}

static inline bool is_connected() { return true; }

static void vTaskShell(void *pvParams) {
    auto *shell = static_cast<Shell *>(pvParams);

    for (;;) {
        print_welcome();

        shell->reset();
        shell->start();

        while (is_connected()) {
            int c = getchar();
            if (c < 0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                shell->update(c);
            }
        }
    }
}

/*-----------------------------------------------------------*/


void vTaskInit(__unused void *pvParams) {
    vTaskSuspendAll();
    // net_init();
    xTaskResumeAll();

    vTaskDelete(nullptr);
}
/*-----------------------------------------------------------*/

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    platform_stdio_init();

    sctrl_init();
    sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ);

    printf("FreeRTOS starting...\r\n");
    busy_wait_ms(10);

    // clang-format off

    // task watchdog
    xTaskCreateStatic(
        vTaskWdt,
        "watchdog",
        count_of(wdtTaskStack),
        nullptr,
        configMAX_PRIORITIES - 2,
        wdtTaskStack,
        &(wdtTaskTCB)
    );

    Shell console(shell_handlers);

    // task shell
    // Outlives main() on the SVC stack: vTaskStartScheduler() never returns, so the
    // SVC stack pointer freezes below this frame and exception frames grow away
    // from it.
    xTaskCreateStatic(
        vTaskShell,
        "shell",
        count_of(shellTaskStack),
        &console,
        configMAX_PRIORITIES - 2,
        shellTaskStack,
        &(shellTaskTCB)
    );

    // clang-format on

    // task init
    xTaskCreate(vTaskInit, "init", configMINIMAL_STACK_SIZE * 2, nullptr, configMAX_PRIORITIES - 1, nullptr);

    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}

#if (configCHECK_FOR_STACK_OVERFLOW == 2)
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    portENTER_CRITICAL();
    // wdt_down();
    printf("STACK OVERFLOW: %s\n", pcTaskName);
    portEXIT_CRITICAL();
    wdt_up();
    for (;;);
}
#endif
