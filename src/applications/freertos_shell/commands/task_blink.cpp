#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/Shell.h"
#include "shell/console_colors.h"
#include "hardware/gpio.h"


#define LED_PIN         26
#define count_of(x)     (sizeof(x) / sizeof(x[0]))
#define TASK_NAME       "blink"


static TaskHandle_t pxTaskBlink = NULL;
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

static void usage() {
    printf("\r\n");
    printf("Usage:\r\n");
    printf("    blink start\r\n");
    printf("    blink stop\r\n");
    printf("    blink stop all\r\n");
    printf("\r\n");
}

int command_blink(int argc, const char *argv[]) {
    if (argc < 2) {
        printf(COLOR_RED("Arguments required") "\r\n");
        usage();
        return 1;
    }

    if (strcmp(argv[1], "start") == 0) {
        auto result = xTaskCreate(
            vTaskBlink,
            TASK_NAME,
            32,
            NULL,
            configMAX_PRIORITIES - 2,
            &pxTaskBlink
        );

        if (result == pdTRUE) {
            run = true;
            printf(COLOR_GREEN("Task created") "\r\n");
        } else {
            printf(COLOR_RED("Task not created") "\r\n");
            return 1;
        }

        return 0;
    }

    if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        if (pxTaskBlink) {
            vTaskDelete(pxTaskBlink);
            printf("done\r\n");
            run = false;
            pxTaskBlink = NULL;
        } else {
            printf(COLOR_RED("task not found\r\n"));
        }

        return 0;
    }

    if (argc == 3 && strcmp(argv[1], "stop") == 0 && strcmp(argv[2], "all") == 0) {
        uint32_t uxArrayLength = uxTaskGetNumberOfTasks();
        size_t xTasksBufferSize = (sizeof(TaskStatus_t) * uxArrayLength);
        TaskStatus_t *pxTasksBuffer = static_cast<TaskStatus_t *>(pvPortMalloc(xTasksBufferSize));

        if (!pxTasksBuffer) {
            printf(COLOR_RED("Can't allocate %d bytes") "\r\n", xTasksBufferSize);
            return 3;
        }

        size_t uxDeletedCount = 0;
        uxArrayLength = uxTaskGetSystemState(pxTasksBuffer, uxArrayLength, NULL);
        if (uxArrayLength > 0) {
            for (unsigned int i = 0; i < uxArrayLength; i++) {
                if (strcmp(pxTasksBuffer[i].pcTaskName, TASK_NAME) != 0) continue;
                vTaskDelete(pxTasksBuffer[i].xHandle);
                uxDeletedCount++;
            }
        }
        vPortFree(pxTasksBuffer);

        if (uxDeletedCount > 0) {
            printf("%d tasks stopped\r\n", uxDeletedCount);
        } else {
            printf("required tasks not found\r\n");
        }

        return 0;
    }

    printf(COLOR_RED("Invalid required") "\r\n");
    usage();
    return 1;
}
