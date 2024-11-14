#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/Console.h"
#include "hardware/gpio.h"


#define LED_PIN         26
#define count_of(x)     (sizeof(x) / sizeof(x[0]))


static TaskHandle_t pxTaskBlink;
static bool run = false;

static void vTaskBlink(__unused void *pvParams) {
    gpio_config(LED_PIN, GPIO_OUT);

    for (;;) {
        gpio_put(LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void command_blink(Console &c) {
    const char *pAction = c.packet.take_rest_string();

    if (strcmp(pAction, "start") == 0) {
        auto result = xTaskCreate(
                vTaskBlink,
                "blink",
                configMINIMAL_STACK_SIZE,
                NULL,
                configMAX_PRIORITIES - 2,
                &pxTaskBlink
        );

        if (result == pdTRUE) {
            run = true;
            printf(COLOR_GREEN("Task created") "\r\n");
        } else {
            printf(COLOR_RED("Task not created") "\r\n");
        }

    } else if (strcmp(pAction, "stop") == 0) {
        vTaskDelete(pxTaskBlink);
        printf("done\r\n");
        run = false;

    } else {
        printf(COLOR_RED("Action is not defined") "\r\n");
        printf("Usage: %s <%s|%s>\r\n", "blink", "start", "stop");
        return;
    }
}
