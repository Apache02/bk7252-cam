#include "shell_handlers.h"
#include <stdio.h>

#include "shell/commands_common.h"
#include "shell/commands_platform.h"
#include "shell/commands_freertos.h"
#include "shell/commands_beken.h"
#include "shell/commands_wifi.h"
#include "shell/Shell.h"


static int help(__unused int intc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help", help, nullptr},
    {"reboot", command_reboot, nullptr},
    {"dump32", command_dump32, nullptr},
    {"chip_id", command_chip_id, nullptr},
    {"partitions", command_partitions, nullptr},
    {"stack", command_stack, nullptr},
    {"tasks", command_tasks, nullptr},
    {"free", command_free, nullptr},
    {"go", command_jump, nullptr},
    {"wifi", command_wifi, nullptr},
    {"ping", command_ping, nullptr},
    // required at the end
    {nullptr, nullptr, nullptr},
};
