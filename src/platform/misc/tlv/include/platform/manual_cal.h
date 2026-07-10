#ifndef _TLV_MANUAL_CAL_H
#define _TLV_MANUAL_CAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Largest payload any known field decoder inspects (TXPWR_TAB_BLE_ID, 40
// bytes, is the largest leaf; rounded up for headroom).
#define TLV_MANUAL_CAL_MAX_PAYLOAD 64u

// True for TXID (0x11111100) and TXPWR_TAB_TAB (0x22222200) -- the two known
// container types in the manual_cal blob (see manual_cal_bk7231U.c in the
// vendor SDK, BK_FLASH_OPT_TLV_HEADER format).
bool tlv_manual_cal_is_container(uint32_t type);

// Returns the field name for any known type (leaf or container), or NULL if
// `type` is not in the schema table.
const char *tlv_manual_cal_name(uint32_t type);

// Formats a known leaf field's payload into out (out_size bytes,
// NUL-terminated). `payload`/`len` must already be read by the caller (up to
// TLV_MANUAL_CAL_MAX_PAYLOAD bytes are inspected; longer payloads are
// silently truncated for decoding purposes only). Returns false if `type` is
// not a known leaf field (unknown type, or a container type which has no
// leaf value) -- caller should fall back to a raw hex dump in that case.
bool tlv_manual_cal_decode(uint32_t type, const uint8_t *payload, uint32_t len, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // _TLV_MANUAL_CAL_H
