#include <stdio.h>
#include "platform/fal.h"


#define KB 1024


__section(".fal_partition_table") __used const fal_partition_t partition_table_def[] = {
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "bootloader",
        .flash_name = FLASH_BEKEN_ONCHIP_CRC,
        .offset     = 0x00000000,
        .len        = 60 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "app",
        .flash_name = FLASH_BEKEN_ONCHIP_CRC,
        .offset     = 0x00010000,
        .len        = 1152 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "download",
        .flash_name = FLASH_BEKEN_ONCHIP,
        .offset     = 0x132000,
        .len        = 696 * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "rf_param",
        .flash_name = FLASH_BEKEN_ONCHIP,
        .offset     = 0x10000,
        .len        = 0x1000,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "unused",
        .flash_name = FLASH_BEKEN_ONCHIP,
        .offset     = 0x1e0000,
        .len        = (128 - 8) * KB,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "param1",
        .flash_name = FLASH_BEKEN_ONCHIP,
        .offset     = 0x1fe000,
        .len        = 0x1000,
        .reserved   = 0,
    },
    {
        .magic_word = FAL_PART_MAGIC_WORD,
        .name       = "param2",
        .flash_name = FLASH_BEKEN_ONCHIP,
        .offset     = 0x1ff000,
        .len        = 0x1000,
        .reserved   = 0,
    },
    /* terminator — magic_word != FAL_PART_MAGIC_WORD stops iteration */
    {
        .magic_word = 0,
    },
};
