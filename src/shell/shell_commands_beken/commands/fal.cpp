#include "shell/commands_beken.h"
#include <stdio.h>
#include <stdint.h>


#define FAL_PART_MAGIC_WORD         0x45503130
#define FAL_DEV_NAME_MAX            24
#define FAL_PART_TABLE_START        0xe000
#define FAL_PART_TABLE_END          0xf000


typedef struct {
    uint32_t magic_word;
    char name[FAL_DEV_NAME_MAX];
    char flash_name[FAL_DEV_NAME_MAX];
    long offset;
    size_t len;
    uint32_t reserved;
} fal_partition_t;


static const fal_partition_t *find_partition_table() {
    for (uint32_t addr = FAL_PART_TABLE_START; addr < FAL_PART_TABLE_END; addr += sizeof(uint32_t)) {
        if (*reinterpret_cast<const uint32_t *>(addr) == FAL_PART_MAGIC_WORD) {
            return reinterpret_cast<const fal_partition_t *>(addr);
        }
    }
    return nullptr;
}

static void format_size(char *buf, size_t buf_len, size_t size) {
    if (size >= 1024 * 1024) {
        unsigned whole = size / (1024 * 1024);
        unsigned frac  = (size % (1024 * 1024)) * 100 / (1024 * 1024);
        snprintf(buf, buf_len, "%u.%02u MB", whole, frac);
    } else if (size >= 1024) {
        unsigned whole = size / 1024;
        unsigned frac  = (size % 1024) * 100 / 1024;
        snprintf(buf, buf_len, "%u.%02u KB", whole, frac);
    } else {
        snprintf(buf, buf_len, "%u B", (unsigned)size);
    }
}

int command_partitions(__unused int argc, __unused const char *argv[]) {
    const fal_partition_t *tbl = find_partition_table();

    if (!tbl) {
        printf("Error: Partitions table not found!\r\n");
        return 1;
    }

    printf("| %16s | %16s | %10s | %10s | %10s |\r\n",
           "name", "flash name", "addr", "size (hex)", "size"
    );
    printf("| ---------------- | ---------------- | ---------- | ---------- | ---------- |\r\n");

    char size_hex[16];
    char size_human[16];
    for (; tbl->magic_word == FAL_PART_MAGIC_WORD; tbl++) {
        sprintf(size_hex, "0x%x", tbl->len);
        format_size(size_human, sizeof(size_human), tbl->len);
        printf("| %16s | %16s | 0x%08lx | %10s | %10s |\r\n",
               tbl->name, tbl->flash_name, tbl->offset, size_hex, size_human
        );
    }

    return 0;
}
