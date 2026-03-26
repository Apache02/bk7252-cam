#ifndef _UTILS_CRC16_H
#define _UTILS_CRC16_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
#endif
uint16_t crc16(const unsigned char *buf, size_t len);


#endif // _UTILS_CRC16_H
