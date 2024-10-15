#include "handlers.h"
#include "commands/commands.h"
#include <stdio.h>


static void help(Console &console) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!handlers[i].name || !handlers[i].handler) {
            break;
        }

        printf("  %s\r\n", handlers[i].name);
    }
}

const Console::Handler handlers[] = {
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
        {"blink",          command_blink},
        {"tt",             command_timers_test},
        {"tt2",            command_timers_test2},
        // required at the end
        {nullptr,          nullptr},
};
