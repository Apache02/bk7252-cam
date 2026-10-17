#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "platform/stdio.h"
#include "platform/panic.h"
#include "hardware/sctrl.h"
#include "hardware/wdt.h"
#include "hardware/random.h"
#include "utils/busy_wait.h"
#include "utils/ring_buffer.h"

#include "app/main_task.h"
#include "shell_handlers.h"


#define count_of(x) (sizeof(x) / sizeof(x[0]))


void print_version() { printf("bk7252-cam by Apache02\r\n\r\n"); }

/*-----------------------------------------------------------*/
// Watchdog task

static StaticTask_t wdtTaskTCB;
static StackType_t  wdtTaskStack[40];

static __dead2 void vTaskWdt(__unused void *pvParams) {
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

static void print_welcome() {
    printf("\r\n");
    printf("Shell is ready");
    printf("\r\n\n");
}

static inline bool is_connected() { return true; }

static __dead2 void vTaskShell(void *pvParams) {
    auto *shell = static_cast<Shell *>(pvParams);

    for (int i=0; i<5 && !main_task_ready(); i++) {
        vTaskDelay(pdMS_TO_TICKS(200));
    }

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

RINGBUF_DECLARE(uart2_tx, 1024);

int main() {
    trng_enable();
    wdt_down();
    wdt_set(2000);

    setvbuf(stdout, nullptr, _IONBF, 0);
    platform_stdio_init();
    platform_stdio_set_tx_buffer(&uart2_tx);
    print_version();

    sctrl_init();
    sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ); // required by Wi-Fi

    busy_wait_ms(10);

    // Outlives main() on the SVC stack: vTaskStartScheduler() never returns, so the
    // SVC stack pointer freezes below this frame and exception frames grow away
    // from it.
    Shell console(shell_handlers);

    // clang-format off
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

    main_task_start();

    // clang-format off
    xTaskCreateStatic(
        vTaskWdt,
        "watchdog",
        count_of(wdtTaskStack),
        nullptr,
        configMAX_PRIORITIES - 4,
        wdtTaskStack,
        &(wdtTaskTCB)
    );
    // clang-format on

    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}
