#pragma once

// AES-128 in software (both directions) and the RFC 3394 key wrap, which is all
// the handshake needs: the key data of messages 3 and group 1 is wrapped under
// the KEK. The tables are built on first use, so call it from one task.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// in and out may be the same buffer.
void wpa2_aes128_encrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);
void wpa2_aes128_decrypt(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

// plain_len is a multiple of 8 and at least 16; out takes plain_len + 8 bytes.
void wpa2_aes_key_wrap(const uint8_t kek[16], const uint8_t *plain, size_t plain_len, uint8_t *out);

// in_len is a multiple of 8 and at least 24; out takes in_len - 8 bytes. False
// when the length is wrong or the integrity check fails.
bool wpa2_aes_key_unwrap(const uint8_t kek[16], const uint8_t *in, size_t in_len, uint8_t *out);

#ifdef __cplusplus
}
#endif
