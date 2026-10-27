#include "app/status_led.h"

#include <stdio.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "board.h"
#include "hardware/gpio.h"
#include "wifi/ap.h"
#include "wifi/sta.h"

#include "app/config.h"
#include "app/main_task.h"

#define count_of(x) (sizeof(x) / sizeof(x[0]))


static volatile bool s_hold = false;

// A board without LED_PIN has no status LED: the task is not started.
#ifdef LED_PIN
#define LED_ON  (!LED_PIN_INVERTED)
#define LED_OFF (LED_PIN_INVERTED)

static __dead2 void status_led_task(__unused void *params) {
    gpio_config(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, LED_OFF);

    gpio_put(LED_PIN, LED_ON);
    await_main_task_ready(1000);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_put(LED_PIN, LED_OFF);

    int delay = 1000;

    for (;;) {
        if (s_hold) {
            gpio_put(LED_PIN, LED_ON);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (delay > 0) {
            delay -= 100;
            gpio_put(LED_PIN, LED_OFF);
            vTaskDelay(pdMS_TO_TICKS(100));
        } else if (!g_config.led_enabled) {
            gpio_put(LED_PIN, LED_OFF);
            delay = 2000;
        } else if (g_config.sta_enabled && g_config.sta_ssid[0] != '\0') {
            if (wifi_sta_get_address(nullptr, nullptr, nullptr)) {
                gpio_put(LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_put(LED_PIN, LED_OFF);
                delay = 2000 - 20;
            } else {
                gpio_put(LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_put(LED_PIN, LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(100));
                gpio_put(LED_PIN, LED_ON);
                vTaskDelay(pdMS_TO_TICKS(20));
                gpio_put(LED_PIN, LED_OFF);
                delay = 2000 - 140;
            }
        } else if (wifi_ap_is_up()) {
            gpio_put(LED_PIN, LED_ON);
            vTaskDelay(pdMS_TO_TICKS(500));
            gpio_put(LED_PIN, LED_OFF);
            vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            gpio_put(LED_PIN, LED_OFF);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

static StaticTask_t taskTCB;
static StackType_t  taskStack[configMINIMAL_STACK_SIZE * 4];
#endif // LED_PIN

void status_led_hold(const bool on) { s_hold = on; }

void status_led_start() {
#ifdef LED_PIN
    // clang-format off
    xTaskCreateStatic(
        status_led_task,
        "status_led",
        count_of(taskStack),
        nullptr,
        configMAX_PRIORITIES - 4,
        taskStack,
        &taskTCB
    );
    // clang-format on
#endif
}
