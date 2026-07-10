#include "platform/fal.h"
#include "hardware/flash.h"
#include <string.h>


#define FAL_PARTITION_TABLE_SCAN_FROM 0xd000u
#define FAL_PARTITION_TABLE_SCAN_TO   0xf000u


const fal_partition_t *fal_partition_table(void) {
    for (uint32_t addr = FAL_PARTITION_TABLE_SCAN_FROM; addr < FAL_PARTITION_TABLE_SCAN_TO; addr += sizeof(uint32_t)) {
        if (*(const uint32_t *)addr == FAL_PART_MAGIC_WORD) {
            return (const fal_partition_t *)addr;
        }
    }
    return NULL;
}

const fal_partition_t *fal_find_partition(const char *name) {
    const fal_partition_t *p = fal_partition_table();
    if (!p) return NULL;

    for (; p->magic_word == FAL_PART_MAGIC_WORD; p++) {
        if (strncmp(p->name, name, FAL_DEV_NAME_MAX) == 0) return p;
    }
    return NULL;
}

int fal_read(const fal_partition_t *part, uint32_t offset, uint8_t *buf, uint32_t len) {
    if (offset + len > part->len) return -1;

    if (strcmp(part->flash_name, FLASH_BEKEN_ONCHIP_CRC) == 0) {
        memcpy(buf, (void *)offset + part->offset, len);
    } else if (strcmp(part->flash_name, FLASH_BEKEN_ONCHIP) == 0) {
        flash_read((uint32_t)part->offset + offset, buf, len);
    } else {
        flash_read((uint32_t)part->offset + offset, buf, len);
    }

    return 0;
}
