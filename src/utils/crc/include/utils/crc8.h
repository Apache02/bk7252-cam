#ifndef _UTILS_CRC8_H
#define _UTILS_CRC8_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t crc8_lsb(const unsigned char *buf, size_t len);

uint8_t crc8_msb(const unsigned char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif // _UTILS_CRC8_H
