#include "commands.h"
#include <stdio.h>


#define FAL_PART_MAGIC_WROD         0x45503130
#define FAL_DEV_NAME_MAX            24

struct fal_partition {
    uint32_t magic_word;
    char name[FAL_DEV_NAME_MAX];
    char flash_name[FAL_DEV_NAME_MAX];
    long offset;
    size_t len;
    uint32_t reserved;
};

void command_partitions(Console &c) {
    // find partitions table
    struct fal_partition *tbl = NULL;
    for (uint32_t addr = 0xe000; addr < 0xf000; addr += sizeof(tbl->magic_word)) {
        if (*((uint32_t *) addr) == FAL_PART_MAGIC_WROD) {
            tbl = reinterpret_cast<fal_partition *>(addr);
            break;
        }
    }

    if (!tbl) {
        printf("Error: Partitions table not found!\r\n");
        return;
    }


    printf("| %16s | %16s | %10s | %10s |\r\n", "name", "flash name", "addr", "size");
    printf("| ---------------- | ---------------- | ---------- | ---------- |\r\n");
    char tmp_len[16] = {0};
    for (; tbl->magic_word == FAL_PART_MAGIC_WROD; tbl++) {
        sprintf(tmp_len, "0x%x", tbl->len);
        printf("| %16s | %16s | 0x%08lx | %10s |\r\n", tbl->name, tbl->flash_name, tbl->offset, tmp_len);
    }
}

