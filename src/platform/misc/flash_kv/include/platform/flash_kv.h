#ifndef _PLATFORM_FLASH_KV_H
#define _PLATFORM_FLASH_KV_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// The sector the store lives in.
#define FLASH_KV_ADDR 0x1FF000u
#define FLASH_KV_SIZE 4096u

#define FLASH_KV_MAX_VALUE_LEN 256

// Reserved: marks the end of the log, never a valid key.
#define FLASH_KV_KEY_NONE 0xFFFF

typedef enum {
    FLASH_KV_BLANK,   // header is all 0xFF
    FLASH_KV_FOREIGN, // header is not ours, the first write erases the sector
    FLASH_KV_VALID,
    FLASH_KV_TORN, // a broken record ends the log, the next write compacts
} flash_kv_state_t;

typedef enum {
    FLASH_KV_RECORD_LIVE,    // the newest record of its key
    FLASH_KV_RECORD_OLD,     // replaced by a newer record of the same key
    FLASH_KV_RECORD_DELETED, // the newest record of its key, and it removes the key
} flash_kv_record_status_t;

typedef struct {
    uint32_t                 offset; // from the start of the sector
    uint16_t                 key;
    uint16_t                 len;
    const uint8_t           *value; // valid only during the callback
    flash_kv_record_status_t status;
} flash_kv_record_t;

typedef void (*flash_kv_visit_fn)(void *ctx, const flash_kv_record_t *record);

#ifdef __cplusplus
extern "C" {
#endif

flash_kv_state_t flash_kv_state(void);

// Calls visit for every valid record in the order they were written, including replaced and
// deleted ones. *used receives the end of the last valid record, or 0 if the log is blank or foreign.
flash_kv_state_t flash_kv_foreach(flash_kv_visit_fn visit, void *ctx, uint32_t *used);

// Copies the value of key into buf and returns its length. A NULL buf only returns the length.
// A nonzero exact_size is the only length the stored value may have, so a number is never read from
// a value of another size; 0 accepts any length, as for a string.
// Returns -ENOENT if the key is not set, -EBADMSG if the length differs from exact_size and
// -EMSGSIZE if the value does not fit in size. On any error buf is left untouched.
int flash_kv_get(uint16_t key, void *buf, size_t size, size_t exact_size);

// Typed reads. They return 0 (the string one returns the length) or the error of flash_kv_get(); on
// an error the destination keeps what it had, so it can hold a default. A number is read only from a
// value of its own size. The string is stored without its terminator and always comes back terminated.
int flash_kv_get_u8(uint16_t key, uint8_t *dst);
int flash_kv_get_bool(uint16_t key, bool *dst);
int flash_kv_get_string(uint16_t key, char *dst, size_t size);

// Typed writes, with the return values and the no-op for an unchanged value of flash_kv_set().
// An empty string removes the key.
int flash_kv_set_u8(uint16_t key, uint8_t value);
int flash_kv_set_bool(uint16_t key, bool value);
int flash_kv_set_string(uint16_t key, const char *src);

// Stores a value of up to FLASH_KV_MAX_VALUE_LEN bytes. Storing the value that is already there writes nothing.
// A zero len is the same as flash_kv_erase_key().
// Returns 0, -EINVAL, -ENOSPC (the sector cannot hold all keys) or -EIO (read-back mismatch).
int flash_kv_set(uint16_t key, const void *data, size_t len);

// Removes a key. Removing a key that is not set does nothing.
int flash_kv_erase_key(uint16_t key);

// Erases the whole sector, so every key is gone. Returns 0 or -EIO if the sector is not blank
// afterward, for example when the flash is protected.
int flash_kv_erase(void);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_FLASH_KV_H
