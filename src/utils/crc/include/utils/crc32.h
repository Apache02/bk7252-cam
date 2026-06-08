#ifndef _UTILS_CRC32_H
#define _UTILS_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* One-shot CRC32 over a complete buffer. */
uint32_t crc32(const unsigned char *buf, size_t len);

/* Incremental CRC32 — use when data arrives in chunks:
 *   uint32_t crc = crc32_init();
 *   crc = crc32_update(crc, chunk1, len1);
 *   crc = crc32_update(crc, chunk2, len2);
 *   result = crc32_final(crc);
 */
uint32_t crc32_init(void);
uint32_t crc32_update(uint32_t crc, const uint8_t *buf, size_t len);
uint32_t crc32_final(uint32_t crc);

#ifdef __cplusplus
}
#endif

#endif // _UTILS_CRC32_H
