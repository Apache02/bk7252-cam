#include "shell_handlers.h"
#include "shell/commands_common.h"
#include "shell/commands_beken.h"
#include "commands/commands.h"
#include <stdio.h>


static int help(__unused int intc, __unused const char *argv[]) {
    printf("Commands:\r\n");
    for (int i = 0;; i++) {
        if (!shell_handlers[i].name || !shell_handlers[i].handler) break;

        if (shell_handlers[i].description) {
            printf("  %-16s %s\r\n", shell_handlers[i].name, shell_handlers[i].description);
        } else {
            printf("  %s\r\n", shell_handlers[i].name);
        }
    }
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help", help, nullptr},
    {"echo", command_echo, nullptr},
    {"reboot", command_reboot, nullptr},
    {"chip_id", command_chip_id, nullptr},
    {"dump", command_dump, nullptr},
    {"dump32", command_dump32, nullptr},
    {"watch_reg", command_watch_reg, nullptr},
    {"partitions", command_partitions, nullptr},
    {"timer_delay", command_time_delay, nullptr},
    {"cpu_speed", command_cpu_speed, nullptr},
    {"sys_clk_source", command_sys_clk_source, nullptr},
    {"sys_clk_div", command_sys_clk_div, nullptr},
    {"sys_ctl_print", comand_sys_ctl_print, nullptr},
    {"sys_ctl_test", comand_sys_ctl_test, nullptr},
    {"pwm", command_pwm, nullptr},
    {"pwm_stop", command_pwm_stop, nullptr},
    {"sos", command_sos, nullptr},
    {"gpio_blink", command_gpio_blink, nullptr},
    {"tt", command_timers_test, nullptr},
    {"tt2", command_timers_test2, nullptr},
    {"stack", command_stack, nullptr},
    {"write_regs", command_write_regs, nullptr},
    // required at the end
    {nullptr, nullptr, nullptr},
};
