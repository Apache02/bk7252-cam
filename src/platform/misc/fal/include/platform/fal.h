#ifndef _PLATFORM_FAL_H
#define _PLATFORM_FAL_H

#include <stdint.h>

#define FAL_PART_MAGIC_WORD 0x45503130u // "01PE"
#define FAL_DEV_NAME_MAX    24

#define FLASH_BEKEN_ONCHIP_CRC "beken_onchip_crc" // XIP
#define FLASH_BEKEN_ONCHIP     "beken_onchip"

typedef struct {
    uint32_t magic_word;
    char     name[FAL_DEV_NAME_MAX];
    char     flash_name[FAL_DEV_NAME_MAX];
    long     offset;
    uint32_t len;
    uint32_t reserved;
} fal_partition_t;

#ifdef __cplusplus
extern "C" {
#endif

// Returns a pointer to the first partition table entry, or NULL if no table
// is found (scans the fixed bootloader-region range where partition_table.c
// places the table via bootloader.lds). Entries are consecutive; the list
// ends at the first entry whose magic_word != FAL_PART_MAGIC_WORD.
const fal_partition_t *fal_partition_table(void);

// Linear search by name over fal_partition_table(). Returns NULL if no
// partition table is found, or no entry matches `name`.
const fal_partition_t *fal_find_partition(const char *name);

// Bounds-checked read of partition contents. Fails (returns -1, buf
// untouched) if offset + len > part->len. On success, reads len bytes
// starting at part->offset + offset via flash_read() and returns 0.
int fal_read(const fal_partition_t *part, uint32_t offset, uint8_t *buf, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_FAL_H
