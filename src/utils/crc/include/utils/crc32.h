#ifndef _UTILS_CRC32_H
#define _UTILS_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
#endif
uint32_t crc32(const unsigned char *buf, size_t len);


#endif // _UTILS_CRC32_H
