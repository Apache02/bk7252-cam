#include "commands.h"
#include <stdio.h>

#include "utils/busy_wait.h"
#include "hardware/wdt.h"
#include "hardware/sctrl.h"


void command_reboot(Console &c) {
    int delay = c.packet.take_int().ok_or(0);
    if (delay > 9) delay = 9;

    if (delay > 0) {

        printf("rebooting ... %d", delay);
        while (delay > 0) {
            printf("\b%d", delay);

            busy_wait(1);
            delay--;
        }
    }


    printf("\r\nreboot system\r\n");
    busy_wait_ms(100);

    wdt_reboot(100);
}


struct id_name_map {
    uint32_t id;
    const char *name;
};

const struct id_name_map chip_id_map[] = {
        {0x7111,  "DEFAULT"},
        {0x7231C, "BK7231N"},
        {0x7221a, "BK7251"},
        {0x7231a, "BK7231U"},
        {0x7238,  "BK7238"},
        {0,       "unknown"},
};

const struct id_name_map device_id_map[] = {
        {0x20150414, "DEFAULT"},
        {0x20521023, "BK7231N_A"},
        {0x20521024, "BK7231N_B"},
        {0x20521025, "BK7231N_C"},
        {0x20521026, "BK7231N_D"},
        {0x20521027, "BK7231N_E"},
        {0x20521028, "BK7231N_F"},
        {0x20521029, "BK7231N_G"},
        {0x20521010, "BK7231N_N"},
        {0x20521011, "BK7231N_O"},
        {0x20A21020, "BK7231N_P"},
        {0x20A21021, "BK7231N_P_B"},
        {0x18221020, "BK7251_A"},
        {0x18521020, "BK7231U_A"},
        {0x21128000, "BK7238_MPW"},
        {0x22068000, "BK7238_A"},
        {0,          "unknown"},
};

static const char *get_name_by_id(const struct id_name_map *map, uint32_t id) {
    for (int i = 0;; i++) {
        if (map[i].id == id || map[i].id == 0) {
            return device_id_map[i].name;
        }
    }

    return "error";
}

void command_chip_id(Console &c) {
    uint32_t cid = chip_id();
    uint32_t did = device_id();

    printf("chip id: 0x%lx | %s\r\n", cid, get_name_by_id(chip_id_map, cid));
    printf(" dev id: 0x%lx | %s\r\n", did, get_name_by_id(device_id_map, did));
}

