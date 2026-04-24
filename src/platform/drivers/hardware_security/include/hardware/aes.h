#ifndef _HARDWARE_AES_H
#define _HARDWARE_AES_H

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

// Key length in bytes: 16 for AES-128, 24 for AES-192, 32 for AES-256.
// Block length is always 16 bytes.

void *aes128_encrypt_create_context(const uint8_t *key);

void *aes192_encrypt_create_context(const uint8_t *key);

void *aes256_encrypt_create_context(const uint8_t *key);

void *aes128_decrypt_create_context(const uint8_t *key);

void *aes192_decrypt_create_context(const uint8_t *key);

void *aes256_decrypt_create_context(const uint8_t *key);

void aes_destroy_context(void *ctx);

// Process an integer number of 16-byte blocks in ECB mode.
// length must be a multiple of 16; returns 0 on success, -1 otherwise.
// input and output may alias (same pointer allowed).
int aes_update(void *ctx, const uint8_t *input, uint8_t *output, size_t length);

// One-shot helpers: length must be a multiple of 16.
// Return 0 on success, -1 if length is not a multiple of 16.
int aes128_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);
int aes192_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);
int aes256_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);
int aes128_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);
int aes192_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);
int aes256_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_AES_H