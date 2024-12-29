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

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_FLASH_H
