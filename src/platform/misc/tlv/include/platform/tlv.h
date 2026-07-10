#ifndef _PLATFORM_TLV_H
#define _PLATFORM_TLV_H

#include <stdint.h>
#include <stdbool.h>

#define TLV_MAGIC 0x00564c54u  // "TLV\0"

#define TLV_OK              0
#define TLV_ERR_NO_MAGIC   (-1)
#define TLV_ERR_TOO_LARGE  (-2)
#define TLV_ERR_READ       (-3)

#ifdef __cplusplus
extern "C" {
#endif

// Reads len bytes into buf starting at offset (in whatever address space
// read_ctx represents — a partition, a raw flash address, an in-RAM buffer,
// etc). Returns 0 on success, non-zero on failure.
typedef int (*tlv_read_fn)(void *read_ctx, uint32_t offset, uint8_t *buf, uint32_t len);

typedef struct {
    uint32_t type;
    uint32_t len;             // payload length in bytes
    uint32_t payload_offset;  // offset (same address space as tlv_read_fn) where the payload starts
    uint32_t depth;           // 0 = top level, increases for nested container entries
} tlv_entry_t;

// Returns true if `type` is a container whose payload is itself a nested
// sequence of TLV entries (rather than raw leaf data).
typedef bool (*tlv_is_container_fn)(uint32_t type);

typedef void (*tlv_visit_fn)(const tlv_entry_t *entry, void *visit_ctx);

// Reads the 8-byte file header (4-byte magic + 4-byte total_len, both
// little-endian) at `start`. On success (TLV_OK), *total_len_out receives
// the byte length of the entry sequence that follows at `start + 8`.
// Fails with TLV_ERR_NO_MAGIC if the magic doesn't match TLV_MAGIC,
// TLV_ERR_TOO_LARGE if total_len > bound, TLV_ERR_READ if `read` fails.
// `total_len_out` may be NULL.
int tlv_read_header(tlv_read_fn read, void *read_ctx, uint32_t start, uint32_t bound, uint32_t *total_len_out);

// Walks a flat sequence of type/len/payload entries occupying [start,
// start + len), calling `visit` for each in order. When
// is_container(entry->type) is true, recurses into the entry's payload
// as a nested entry sequence (incrementing depth) before continuing with
// the entry's next sibling.
// Returns TLV_OK, TLV_ERR_TOO_LARGE if an entry's length would run past the
// end of its enclosing range, or TLV_ERR_READ if `read` fails.
int tlv_walk(tlv_read_fn read, void *read_ctx, uint32_t start, uint32_t len,
             tlv_is_container_fn is_container, tlv_visit_fn visit, void *visit_ctx);

#ifdef __cplusplus
}
#endif

#endif // _PLATFORM_TLV_H
