#include <FreeRTOS.h>
#include <task.h>

#include <stdio.h>
#include "shell_handlers.h"


Console console(handlers);

static void print_welcome() {
    printf("\r\n%s.\r\n\r\n", COLOR_WHITE("Console is ready"));
}

static bool is_connected() {
    return true;
}

void vTaskShell(__unused void *pvParams) {
    for (;;) {
        print_welcome();

        console.reset();
        console.start();

        while (is_connected()) {
            int c = getchar();
            if (c < 0) {
                vTaskDelay(pdMS_TO_TICKS(5));
            } else {
                console.update(c);
            }
        }
    }
}
