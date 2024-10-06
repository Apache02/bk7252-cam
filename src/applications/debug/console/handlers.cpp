#include "handlers.h"
#include "commands/commands.h"
#include <stdio.h>


static void help(Console &console) {
    printf("Commands:\r\n");
    for (int i = 0; i < handlers_count; i++) {
        printf("  %s\r\n", handlers[i].name);
    }
}

const ConsoleHandler handlers[] = {
        {"help",           help},
        {"echo",           command_echo},
        {"reboot",         command_reboot},
        {"chip_id",        command_chip_id},
        {"dump",           command_dump},
        {"dump32",         command_dump32},
        {"watch_reg",      command_watch_reg},
        {"partitions",     command_partitions},
        {"timer",          command_timer},
        {"cpu_speed",      command_cpu_speed},
        {"sys_clk_source", command_sys_clk_source},
        {"sys_clk_div",    command_sys_clk_div},
        {"sys_ctl_print",  comand_sys_ctl_print},
        {"sys_ctl_test",   comand_sys_ctl_test},
        {"pwm",            command_pwm},
        {"pwm_stop",       command_pwm_stop},
};

const size_t handlers_count = sizeof(handlers) / sizeof(handlers[0]);
