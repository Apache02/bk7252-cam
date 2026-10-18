#include "shell_handlers.h"
#include <stdio.h>

#include "shell/commands_common.h"
#include "shell/commands_platform.h"
#include "shell/commands_freertos.h"
#include "shell/commands_beken.h"
#include "shell/commands_wifi.h"
#include "shell/commands_telnet.h"
#include "shell/commands_flash_kv.h"
#include "shell/Shell.h"
#include "app/config.h"
#include "app/main_task.h"


static int help(__unused int intc, __unused const char *argv[]) {
    print_command_help(shell_handlers);
    return 0;
}

static int command_config_reset(__unused int argc, __unused const char *argv[]) {
    const int err = config_reset();
    if (err) {
        printf("config reset failed: %d\r\n", err);
        return 1;
    }
    main_task_config_changed();
    const uint8_t *mac = g_config.mac;
    printf("config reset, MAC: %02x:%02x:%02x:%02x:%02x:%02x\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return 0;
}

const Shell::Handler shell_handlers[] = {
    {"help", help, nullptr},
    {"config_reset", command_config_reset, nullptr},
    {"reboot", command_reboot, nullptr},
    {"dump32", command_dump32, nullptr},
    {"flash_dump", command_flash_dump, nullptr},
    {"chip_id", command_chip_id, nullptr},
    {"partitions", command_partitions, nullptr},
    {"stack", command_stack, nullptr},
    {"tasks", command_tasks, nullptr},
    {"free", command_free, nullptr},
    {"go", command_jump, nullptr},
    {"kv", command_flash_kv, nullptr},
    {"wifi", command_wifi, nullptr},
    {"ping", command_ping, nullptr},
    {"exit", command_telnet_exit, nullptr},
    // required at the end
    {nullptr, nullptr, nullptr},
};
