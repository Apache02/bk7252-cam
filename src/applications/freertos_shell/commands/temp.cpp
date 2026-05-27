#include <FreeRTOS.h>
#include <task.h>
#include <stdio.h>
#include "hardware/sctrl_regs.h"


int command_temp_sensor(__unused int argc, __unused const char *argv[]) {
    sctrl_block_enable_temprature_sensor();

    printf("\r\n");

    return 0;
}
