#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "shell/Parser.h"


extern "C" void rsa_explore(const char *action);

int command_rsa(__unused int argc, __unused const char *argv[]) {
    if (argc < 2) {
        printf("Invalid arguments\r\n");
        return -1;
    }

    portDISABLE_INTERRUPTS();
    rsa_explore(argv[1]);
    portENABLE_INTERRUPTS();

    return 0;
}
