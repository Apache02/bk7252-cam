#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "platform/stdio.h"
#include "platform/panic.h"
#include "hardware/sctrl.h"
#include "hardware/wdt.h"
#include "utils/busy_wait.h"
#include "utils/ring_buffer.h"

#include "net.h"
#include "wifi/core.h"


#define count_of(x) (sizeof(x) / sizeof(x[0]))

/*-----------------------------------------------------------*/
// Watchdog task

static StaticTask_t wdtTaskTCB;
static StackType_t  wdtTaskStack[40];

static void vTaskWdt(__unused void *pvParams) {
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

#include "shell_handlers.h"

static StaticTask_t shellTaskTCB;
static StackType_t  shellTaskStack[configMINIMAL_STACK_SIZE * 6];

static void print_welcome() {
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
                // TODO: await UART RX
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                shell->update(c);
            }
        }
    }
}

/*-----------------------------------------------------------*/

static void vTaskInit(__unused void *pvParams) {
    // Starts the tcpip thread. Has to happen before any interface is created,
    net_init();

    // before an interface is created.
    sctrl_dpll_int_open();
    sctrl_rf_init();
    rwnxl_init();
    wifi_core_start();
    sctrl_set_cpu_freq_hz(CPU_FREQ_160_MHZ);

    vTaskDelete(nullptr);
}

/*-----------------------------------------------------------*/

RINGBUF_DECLARE(uart2_tx, 1024);

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    platform_stdio_init();
    platform_stdio_set_tx_buffer(&uart2_tx);

    sctrl_init();

    busy_wait_ms(10);

    // task watchdog
    // clang-format off
    xTaskCreateStatic(
        vTaskWdt,
        "watchdog",
        count_of(wdtTaskStack),
        NULL,
        configMAX_PRIORITIES - 2,
        wdtTaskStack,
        &(wdtTaskTCB)
    );
    // clang-format on

    // task shell
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

    // task init
    xTaskCreate(vTaskInit, "init", configMINIMAL_STACK_SIZE * 2, NULL, configMAX_PRIORITIES - 1, NULL);

    vTaskStartScheduler();

    panic("Scheduler complete\r\n");

    return 0;
}
