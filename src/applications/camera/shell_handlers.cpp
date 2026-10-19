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
    {"help", help, "list the commands"},
    {"reboot", command_reboot, "perform reboot"},
    {"dump32", command_dump32, "32-bit word dump of memory: [addr]"},
    {"flash_dump", command_flash_dump, "hex dump flash: <addr> [size]"},
    {"chip_id", command_chip_id, "print chip and device IDs"},
    {"partitions", command_partitions, "find and print partitions table"},
    {"stack", command_stack, "mode stacks: address, size and usage"},
    {"tasks", command_tasks, "FreeRTOS tasks: state, stack and CPU use"},
    {"free", command_free, "heap statistics"},
    {"go", command_jump, "jump to address: <addr|app|iram|bootloader>"},
    {"kv", command_flash_kv, "flash key-value store: <dump|erase>"},
    {"config_reset", command_config_reset, "reset the camera settings to defaults"},
    {"wifi", command_wifi, "Wi-Fi control; see wifi help"},
    {"ping", command_ping, "ICMP echo: <ip> [-c N] [-W ms]"},
    {"exit", command_telnet_exit, "close the Telnet session"},
    {"i2c", command_i2c, "I2C control; see i2c --help"},
    {"gpio", command_gpio, "GPIO control; run gpio for usage"},
    // required at the end
    {nullptr, nullptr, nullptr},
};
