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

static size_t read_input(char *buf, size_t buf_size) {
    size_t count = 0;

    for (int c; count < buf_size && (c = getchar()) != -1;) {
        buf[count++] = c;
    }

    return count;
}

void vTaskShell(__unused void *pvParams) {
    for (;;) {
        print_welcome();

        console.reset();
        console.start();

        while (is_connected()) {
            char rx[16];
            size_t count = read_input(rx, sizeof(rx));

            if (count > 0) {
                for (int i = 0; i < count; i++) {
                    int c = rx[i];
                    if (c == '\x1B') {
                        c = console.resolve_key(&rx[i], count);
                        i = count;
                    }
                    console.update(c);
                }
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}
