#ifndef _HARDWARE_FLASH_H
#define _HARDWARE_FLASH_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>


#ifdef __cplusplus
extern "C" {
#endif

uint32_t flash_id();

void flash_read(uint32_t addr, uint8_t *dst, uint32_t count);

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
