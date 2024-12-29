#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/sctrl.h"
#include "hardware/flash.h"


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

const struct id_name_map flash_id_map[] = {
        {0x1C7016, "en_25qh32b"},
        {0x1C7015, "en_25qh16b"},
        {0,        "unknown"},
};

const struct id_name_map manufacturer_id_map[] = {
        {0x1C, "Eon Silicon Devices"},
        {0,    "unknown"},
};


static const char *get_name_by_id(const struct id_name_map *map, uint32_t id) {
    for (int i = 0;; i++) {
        if (map[i].id == id || map[i].id == 0) {
            return map[i].name;
        }
    }

    return "error";
}

void command_chip_id(__unused Console &c) {
    uint32_t cid = chip_id();
    uint32_t did = device_id();

    char scid[16];
    char sdid[16];
    sprintf(scid, "0x%lx", cid);
    sprintf(sdid, "0x%lx", did);

    printf(" chip id: %10s | %s\r\n", scid, get_name_by_id(chip_id_map, cid));
    printf("  dev id: %10s | %s\r\n", sdid, get_name_by_id(device_id_map, did));

    printf("\r\n");

    uint32_t fid = flash_id();
    uint8_t capacity = (fid >> 0) & 0xFF;

    printf("flash id: 0x%08lx | %s\r\n", fid, get_name_by_id(flash_id_map, fid));
    printf("    capacity: %d Mb\r\n", (1 << capacity) / (1 << 20));
    printf("    manufacturer: %s\r\n", get_name_by_id(manufacturer_id_map, (fid >> 16) & 0xFF));
}

