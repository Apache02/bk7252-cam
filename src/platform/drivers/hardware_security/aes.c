#include "security_regs.h"
#include "hardware/aes.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


#define AES_BLOCK_SIZE  16


typedef enum {
    AES128 = 0,
    AES256 = 1,
    AES192 = 2,
} AES_MODE;

// Key length in bytes indexed by AES_MODE.
static const uint8_t aes_key_bytes[3] = {
    [AES128] = 16,
    [AES192] = 24,
    [AES256] = 32,
};


typedef struct {
    AES_MODE mode;
    bool encode;
    uint8_t key[32];             // raw key bytes; only key_bytes[mode] are used
} aes_context_t;


static inline uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t) p[0] << 24)
           | ((uint32_t) p[1] << 16)
           | ((uint32_t) p[2] << 8)
           | ((uint32_t) p[3]);
}

static inline void store_be32(uint8_t *p, uint32_t w) {
    p[0] = (uint8_t) (w >> 24);
    p[1] = (uint8_t) (w >> 16);
    p[2] = (uint8_t) (w >> 8);
    p[3] = (uint8_t) w;
}

static inline void wait_ready(void) {
    while (!hw_aes->status.ready) ;
}

// Load direction + mode, push the key, and run INIT to derive the key schedule.
// After this call the engine is ready to consume 16-byte blocks via NEXT.
static void engine_init(const aes_context_t *c) {
    hw_aes->control.v = 0;
    hw_aes->config.v = 0;
    hw_aes->config.encode = c->encode ? 1 : 0;
    hw_aes->config.mode = c->mode;

    int key_words = aes_key_bytes[c->mode] / sizeof(uint32_t);
    for (int i = 0; i < key_words; i++) {
        hw_aes->key_0to7[i] = load_be32(c->key + i * 4);
    }

    hw_aes->control.v = 0;
    hw_aes->control.init = 1;
    wait_ready();
}

static void process_block(const uint8_t *in, uint8_t *out) {
    for (int i = 0; i < 4; i++) {
        hw_aes->block_0to3[i] = load_be32(in + i * 4);
    }

    hw_aes->control.v = 0;
    hw_aes->control.next = 1;
    wait_ready();

    for (int i = 0; i < 4; i++) {
        store_be32(out + i * 4, hw_aes->result_0to3[i]);
    }
}

static void *aes_create_context(AES_MODE mode, bool encode, const uint8_t *key) {
    aes_context_t *ctx = (aes_context_t *) malloc(sizeof(aes_context_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(aes_context_t));
    ctx->mode = mode;
    ctx->encode = encode;
    memcpy(ctx->key, key, aes_key_bytes[mode]);

    engine_init(ctx);

    return ctx;
}

void aes_destroy_context(void *ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(aes_context_t));
    free(ctx);
}

int aes_update(void *ctx, const uint8_t *input, uint8_t *output, size_t length) {
    if (length % AES_BLOCK_SIZE != 0) return -1;
    (void) ctx;  // key schedule already loaded at create; engine holds the state

    size_t blocks = length / AES_BLOCK_SIZE;
    for (size_t i = 0; i < blocks; i++) {
        process_block(input + i * AES_BLOCK_SIZE, output + i * AES_BLOCK_SIZE);
    }
    return 0;
}

void *aes128_encrypt_create_context(const uint8_t *key) { return aes_create_context(AES128, true,  key); }
void *aes192_encrypt_create_context(const uint8_t *key) { return aes_create_context(AES192, true,  key); }
void *aes256_encrypt_create_context(const uint8_t *key) { return aes_create_context(AES256, true,  key); }
void *aes128_decrypt_create_context(const uint8_t *key) { return aes_create_context(AES128, false, key); }
void *aes192_decrypt_create_context(const uint8_t *key) { return aes_create_context(AES192, false, key); }
void *aes256_decrypt_create_context(const uint8_t *key) { return aes_create_context(AES256, false, key); }

static int aes_oneshot(AES_MODE mode, bool encode,
                       const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) {
    if (length % AES_BLOCK_SIZE != 0) return -1;
    void *ctx = aes_create_context(mode, encode, key);
    if (!ctx) return -1;
    int rc = aes_update(ctx, input, output, length);
    aes_destroy_context(ctx);
    return rc;
}

int aes128_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES128, true,  key, input, output, length); }
int aes192_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES192, true,  key, input, output, length); }
int aes256_encrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES256, true,  key, input, output, length); }
int aes128_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES128, false, key, input, output, length); }
int aes192_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES192, false, key, input, output, length); }
int aes256_decrypt(const uint8_t *key, const uint8_t *input, uint8_t *output, size_t length) { return aes_oneshot(AES256, false, key, input, output, length); }