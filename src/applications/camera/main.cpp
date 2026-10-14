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

#include "net.h"
#include "wifi/core.h"
#include "wifi/net.h"
#include "config.h"
#include "shell_handlers.h"
#include "camera_web.h"
#include "telnet_console.h"


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

static StaticTask_t  shellTaskTCB;
static StackType_t   shellTaskStack[configMINIMAL_STACK_SIZE * 6];
static volatile bool s_ready = false;

static void print_welcome() {
    printf("\r\n");
    printf("Shell is ready");
    printf("\r\n\n");
}

static inline bool is_connected() { return s_ready; }

static __dead2 void vTaskShell(void *pvParams) {
    auto *shell = static_cast<Shell *>(pvParams);

    for (int i=0; i<10 && !s_ready; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
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

static void print_mac(const char *name, uint8_t mac[6]) {
    printf("%s MAC: %02x:%02x:%02x:%02x:%02x:%02x\r\n", name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void vTaskInit(__unused void *pvParams) {
    // Starts the tcpip thread; it has to run before any interface is created.
    net_init();

    config_load();

    sctrl_dpll_int_open();
    sctrl_rf_init();
    rwnxl_init();
    wifi_core_start();

    // The address goes to the radio when the interface comes up, so it has to be set before that.
    uint8_t ap_mac[6];
    config_ap_mac(ap_mac);
    wifi_net_set_mac(WIFI_IFACE_STA, g_config.mac);
    wifi_net_set_mac(WIFI_IFACE_AP, ap_mac);
    print_mac("STA", g_config.mac);
    print_mac("AP", ap_mac);

    vTaskDelay(100);
    camera_web_start();
    if (g_config.telnet_enabled) telnet_console_start();

    s_ready = true;

    vTaskDelete(nullptr);
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

    // clang-format off
    xTaskCreate(
        vTaskInit,
        "init",
        configMINIMAL_STACK_SIZE * 2,
        nullptr,
        configMAX_PRIORITIES - 1,
        nullptr
    );
    // clang-format on

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
