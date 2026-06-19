#include <stdint.h>
#include <stdio.h>


#define FAL_PART_MAGIC_WORD 0x45503130UL
#define FAL_DEV_NAME_MAX    24

#define KB 1024


typedef struct {
    uint32_t     magic_word;
    char         name[FAL_DEV_NAME_MAX];
    char         flash_name[FAL_DEV_NAME_MAX];
    long         offset;
    unsigned int len;
    uint32_t     reserved;
} fal_partition_t;

__section(".fal_partition_table") __used const fal_partition_t partition_table_def[] = {
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "bootloader",
        .flash_name = "beken_onchip_crc",
        .offset     = 0x00000000,
        .len        = 64 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "app",
        .flash_name = "beken_onchip_crc",
        .offset     = 0x00010000,
        .len        = 1152 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "download",
        .flash_name = "beken_onchip",
        .offset     = 0x132000,
        .len        = 696 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "unused",
        .flash_name = "beken_onchip",
        .offset     = 0x1e0000,
        .len        = 128 * KB,
        .reserved   = 0,
    },
    /* terminator — magic_word != FAL_PART_MAGIC_WORD stops iteration */
    {
        .magic_word = 0,
    },
};
