#ifndef _HARDWARE_SHA_H
#define _HARDWARE_SHA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *sha1_create_context();

void *sha224_create_context();

void *sha256_create_context();

void *sha384_create_context();

void *sha512_create_context();

void *sha512_224_create_context();

void *sha512_256_create_context();

void sha_destroy_context(void *ctx);

void sha_update(void *ctx, const unsigned char *input, const size_t length);

void sha_finish(void *ctx, uint8_t *sum);

void sha1(const unsigned char *input, size_t length, uint8_t *sum);

void sha224(const unsigned char *input, size_t length, uint8_t *sum);

void sha256(const unsigned char *input, size_t length, uint8_t *sum);

void sha384(const unsigned char *input, size_t length, uint8_t *sum);

void sha512(const unsigned char *input, size_t length, uint8_t *sum);

void sha512_224(const unsigned char *input, size_t length, uint8_t *sum);

void sha512_256(const unsigned char *input, size_t length, uint8_t *sum);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_SHA_H
