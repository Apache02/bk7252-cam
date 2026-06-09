#ifndef _HARDWARE_FLASH_H
#define _HARDWARE_FLASH_H

#include <stdint.h>


#define FLASH_READ_BLOCK_SIZE       (32u)
#define FLASH_READ_BLOCK_MASK       (FLASH_READ_BLOCK_SIZE - 1u)

#define FLASH_SECTOR_SIZE           (4096u)
#define FLASH_SECTOR_MASK           (FLASH_SECTOR_SIZE - 1u)


#ifdef __cplusplus
extern "C" {
#endif

uint32_t flash_id();

void flash_read(uint32_t addr, uint8_t *dst, uint32_t count);

/* Read flash Status Register 1 (SR1). */
uint8_t flash_read_sr1(void);

/* Write flash Status Register 1 (SR1) via WREN + WRSR.
 * Use to set or clear BP (Block Protect) bits. */
void flash_write_sr1(uint8_t sr1);

/* Convenience: clear all SR1 protection bits (write SR1 = 0x00).
 * The vendor bootloader sets BP bits on startup; call before erasing
 * or writing to 0x000000. No-op if SR1 is already 0x00. */
void flash_unprotect(void);

/* Erase one 4 KB sector that contains addr.
 * addr is aligned down to the nearest 4 KB boundary.
 * The caller must ensure the target region is not write-protected. */
void flash_erase_sector(uint32_t addr);

/* Write count bytes from src to flash starting at addr.
 * Unaligned start/end are handled with a read-modify-write of the
 * surrounding 32-byte hardware block.
 * The target region must have been erased (all 0xFF) before writing. */
void flash_write(uint32_t addr, const uint8_t *src, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_FLASH_H
