#include "platform/tlv.h"

#define TLV_ENTRY_HEADER_SIZE 8u  // type(4) + len(4)
#define TLV_FILE_HEADER_SIZE  8u  // magic(4) + total_len(4)

static uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int tlv_read_header(tlv_read_fn read, void *read_ctx, uint32_t start, uint32_t bound, uint32_t *total_len_out) {
    uint8_t header[TLV_FILE_HEADER_SIZE];
    if (read(read_ctx, start, header, TLV_FILE_HEADER_SIZE) != 0) return TLV_ERR_READ;

    if (read_u32le(header) != TLV_MAGIC) return TLV_ERR_NO_MAGIC;

    uint32_t total_len = read_u32le(header + 4);
    if (total_len > bound) return TLV_ERR_TOO_LARGE;

    if (total_len_out) *total_len_out = total_len;
    return TLV_OK;
}

static int walk_range(tlv_read_fn read, void *read_ctx, uint32_t start, uint32_t end, uint32_t depth,
                       tlv_is_container_fn is_container, tlv_visit_fn visit, void *visit_ctx) {
    uint32_t addr = start;

    while (addr < end) {
        uint8_t header[TLV_ENTRY_HEADER_SIZE];
        if (read(read_ctx, addr, header, TLV_ENTRY_HEADER_SIZE) != 0) return TLV_ERR_READ;

        tlv_entry_t entry;
        entry.type           = read_u32le(header);
        entry.len            = read_u32le(header + 4);
        entry.payload_offset = addr + TLV_ENTRY_HEADER_SIZE;
        entry.depth          = depth;

        if (entry.payload_offset + entry.len > end) return TLV_ERR_TOO_LARGE;

        visit(&entry, visit_ctx);

        if (is_container(entry.type)) {
            int rc = walk_range(read, read_ctx, entry.payload_offset, entry.payload_offset + entry.len,
                                 depth + 1, is_container, visit, visit_ctx);
            if (rc != TLV_OK) return rc;
        }

        addr = entry.payload_offset + entry.len;
    }

    return TLV_OK;
}

int tlv_walk(tlv_read_fn read, void *read_ctx, uint32_t start, uint32_t len,
             tlv_is_container_fn is_container, tlv_visit_fn visit, void *visit_ctx) {
    return walk_range(read, read_ctx, start, start + len, 0, is_container, visit, visit_ctx);
}
