#include "platform/flash_kv.h"
#include "hardware/flash.h"
#include "utils/crc32.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// One 4 KB sector at a fixed address, read and written as raw SPI flash (no XIP, no CRC layout).
// Layout: header (magic, version), then 4-byte aligned records (key, len, value, crc32).
#define KV_BASE        FLASH_KV_ADDR
#define KV_SECTOR_SIZE FLASH_KV_SIZE
#define KV_MAGIC       0x564B4C46u
#define KV_VERSION     1u

#define HEADER_SIZE      8u
#define RECORD_HEAD_SIZE 4u
#define RECORD_CRC_SIZE  4u
#define RECORD_MAX_SIZE  (RECORD_HEAD_SIZE + FLASH_KV_MAX_VALUE_LEN + RECORD_CRC_SIZE)

_Static_assert(FLASH_KV_SIZE == FLASH_SECTOR_SIZE, "the store is exactly one erase sector");

#define MAX_RECORDS ((KV_SECTOR_SIZE - HEADER_SIZE) / (RECORD_HEAD_SIZE + RECORD_CRC_SIZE))

typedef void (*visit_fn)(void *ctx, uint16_t key, const uint8_t *value, uint16_t len, uint32_t off);

static uint16_t read_u16le(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint32_t read_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u16le(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = v >> 8;
}

static void write_u32le(uint8_t *p, uint32_t v) {
    write_u16le(p, v & 0xFFFF);
    write_u16le(p + 2, v >> 16);
}

static uint32_t align4(uint32_t n) { return (n + 3u) & ~3u; }

static uint32_t record_size(uint16_t len) { return RECORD_HEAD_SIZE + align4(len) + RECORD_CRC_SIZE; }

static uint32_t record_crc(const uint8_t *head, const uint8_t *value, uint16_t len) {
    uint32_t crc = crc32_init();
    crc          = crc32_update(crc, head, RECORD_HEAD_SIZE);
    crc          = crc32_update(crc, value, len);
    return crc32_final(crc);
}

// Writes a record to dst and returns its size. len must be at most FLASH_KV_MAX_VALUE_LEN.
static uint32_t encode_record(uint8_t *dst, uint16_t key, const uint8_t *value, uint16_t len) {
    const uint32_t size = record_size(len);

    memset(dst, 0, size);
    write_u16le(dst, key);
    write_u16le(dst + 2, len);
    if (len) memcpy(dst + RECORD_HEAD_SIZE, value, len);
    write_u32le(dst + size - RECORD_CRC_SIZE, record_crc(dst, dst + RECORD_HEAD_SIZE, len));
    return size;
}

static void encode_header(uint8_t *dst) {
    write_u32le(dst, KV_MAGIC);
    write_u32le(dst + 4, KV_VERSION);
}

static flash_kv_state_t header_state(void) {
    uint8_t header[HEADER_SIZE];
    flash_read(KV_BASE, header, HEADER_SIZE);

    if (read_u32le(header) == KV_MAGIC && read_u32le(header + 4) == KV_VERSION) return FLASH_KV_VALID;

    for (uint32_t i = 0; i < HEADER_SIZE; i++) {
        if (header[i] != 0xFF) return FLASH_KV_FOREIGN;
    }
    return FLASH_KV_BLANK;
}

// Calls visit for every valid record in order. Returns false if the log ends on a broken record.
// *end receives the offset of the first byte after the last valid record.
static bool walk(visit_fn visit, void *ctx, uint32_t *end) {
    uint8_t *value = malloc(FLASH_KV_MAX_VALUE_LEN);
    assert(value);

    bool     clean = true;
    uint32_t off   = HEADER_SIZE;

    for (;;) {
        *end = off;
        if (off == KV_SECTOR_SIZE) break;

        uint8_t head[RECORD_HEAD_SIZE];
        flash_read(KV_BASE + off, head, RECORD_HEAD_SIZE);

        const uint16_t key = read_u16le(head);
        const uint16_t len = read_u16le(head + 2);
        if (key == FLASH_KV_KEY_NONE) break;
        if (len > FLASH_KV_MAX_VALUE_LEN || off + record_size(len) > KV_SECTOR_SIZE) {
            clean = false;
            break;
        }

        uint8_t crc[RECORD_CRC_SIZE];
        flash_read(KV_BASE + off + RECORD_HEAD_SIZE, value, len);
        flash_read(KV_BASE + off + RECORD_HEAD_SIZE + align4(len), crc, RECORD_CRC_SIZE);
        if (read_u32le(crc) != record_crc(head, value, len)) {
            clean = false;
            break;
        }

        if (visit) visit(ctx, key, value, len, off);
        off += record_size(len);
    }

    free(value);
    return clean;
}

static bool tail_is_blank(uint32_t from) {
    uint8_t block[FLASH_READ_BLOCK_SIZE];

    for (uint32_t off = from; off < KV_SECTOR_SIZE; off += sizeof(block)) {
        const uint32_t n = KV_SECTOR_SIZE - off < sizeof(block) ? KV_SECTOR_SIZE - off : sizeof(block);
        flash_read(KV_BASE + off, block, n);
        for (uint32_t i = 0; i < n; i++) {
            if (block[i] != 0xFF) return false;
        }
    }
    return true;
}

// *end is the offset where the next record goes, valid only for FLASH_KV_VALID.
static flash_kv_state_t scan(uint32_t *end) {
    const flash_kv_state_t header = header_state();
    if (header != FLASH_KV_VALID) return header;

    // A torn write can leave programmed bytes behind a clean-looking end, so the whole tail must be blank.
    if (!walk(NULL, NULL, end) || !tail_is_blank(*end)) return FLASH_KV_TORN;
    return FLASH_KV_VALID;
}

flash_kv_state_t flash_kv_state(void) {
    uint32_t end;
    return scan(&end);
}

struct lookup {
    uint16_t key;
    uint8_t *buf;
    size_t   size;
    int      len;
};

static void lookup_visit(void *ctx, uint16_t key, const uint8_t *value, uint16_t len, uint32_t off) {
    struct lookup *l = ctx;
    (void)off;

    if (key != l->key) return;
    if (len == 0) {
        l->len = -ENOENT;
        return;
    }
    l->len = len;
    if (l->buf) memcpy(l->buf, value, len < l->size ? len : l->size);
}

int flash_kv_get(uint16_t key, void *buf, size_t size) {
    if (key == FLASH_KV_KEY_NONE) return -EINVAL;
    if (header_state() != FLASH_KV_VALID) return -ENOENT;

    struct lookup l = {.key = key, .buf = buf, .size = size, .len = -ENOENT};
    uint32_t      end;
    walk(lookup_visit, &l, &end);

    if (l.len > 0 && buf && size < (size_t)l.len) return -EMSGSIZE;
    return l.len;
}

// The image is a copy of the sector: the sector is erased before its records are written back.
struct image {
    uint8_t *data;
    uint32_t n;
    bool     overflow;
};

// Drops any record of key from the image, then appends the new value unless len is zero.
static bool image_put(struct image *img, uint16_t key, const uint8_t *value, uint16_t len) {
    uint32_t off = HEADER_SIZE;

    while (off < img->n) {
        const uint32_t size = record_size(read_u16le(img->data + off + 2));
        if (read_u16le(img->data + off) == key) {
            memmove(img->data + off, img->data + off + size, img->n - off - size);
            img->n -= size;
        } else {
            off += size;
        }
    }

    if (len == 0) return true;
    if (img->n + record_size(len) > KV_SECTOR_SIZE) return false;
    img->n += encode_record(img->data + img->n, key, value, len);
    return true;
}

static void collect_visit(void *ctx, uint16_t key, const uint8_t *value, uint16_t len, uint32_t off) {
    struct image *img = ctx;
    (void)off;
    if (!image_put(img, key, value, len)) img->overflow = true;
}

// Writes src and reads it back, since a write to a locked or protected region is silently ignored.
static int write_verified(uint32_t off, const uint8_t *src, uint32_t size) {
    uint8_t block[FLASH_READ_BLOCK_SIZE];

    flash_write(KV_BASE + off, src, size);

    for (uint32_t done = 0; done < size; done += sizeof(block)) {
        const uint32_t n = size - done < sizeof(block) ? size - done : sizeof(block);
        flash_read(KV_BASE + off + done, block, n);
        if (memcmp(block, src + done, n) != 0) return -EIO;
    }
    return 0;
}

// Erases the sector and writes back the surviving records plus the new value.
static int rebuild(flash_kv_state_t state, uint16_t key, const uint8_t *value, uint16_t len) {
    struct image img = {.data = malloc(KV_SECTOR_SIZE), .n = HEADER_SIZE};
    uint32_t     end;

    if (!img.data) return -ENOMEM;

    encode_header(img.data);
    if (state == FLASH_KV_VALID || state == FLASH_KV_TORN) walk(collect_visit, &img, &end);

    int err = 0;
    if (img.overflow || !image_put(&img, key, value, len)) {
        err = -ENOSPC;
    } else {
        flash_erase_sector(KV_BASE);
        err = write_verified(0, img.data, img.n);
    }

    free(img.data);
    return err;
}

static int append(uint16_t key, const uint8_t *value, uint16_t len) {
    uint32_t               end;
    const flash_kv_state_t state = scan(&end);

    if (state == FLASH_KV_VALID && end + record_size(len) <= KV_SECTOR_SIZE) {
        uint8_t *record = malloc(RECORD_MAX_SIZE);
        if (!record) return -ENOMEM;

        const uint32_t size = encode_record(record, key, value, len);
        const int      err  = write_verified(end, record, size);
        free(record);
        return err;
    }
    return rebuild(state, key, value, len);
}

int flash_kv_set(uint16_t key, const void *data, size_t len) {
    if (key == FLASH_KV_KEY_NONE || len > FLASH_KV_MAX_VALUE_LEN || (len && !data)) return -EINVAL;
    if (len == 0) return flash_kv_erase(key);
    return append(key, data, (uint16_t)len);
}

int flash_kv_erase(uint16_t key) {
    const int len = flash_kv_get(key, NULL, 0);
    if (len == -ENOENT) return 0;
    if (len < 0) return len;
    return append(key, NULL, 0);
}

// Offset of the newest record of each key, found in the first pass.
struct latest {
    uint16_t key;
    uint16_t off;
};

struct foreach_ctx {
    struct latest    *table;
    uint32_t          count;
    flash_kv_visit_fn visit;
    void             *user;
};

static void latest_visit(void *ctx, uint16_t key, const uint8_t *value, uint16_t len, uint32_t off) {
    struct foreach_ctx *c = ctx;
    (void)value;
    (void)len;

    for (uint32_t i = 0; i < c->count; i++) {
        if (c->table[i].key == key) {
            c->table[i].off = (uint16_t)off;
            return;
        }
    }
    c->table[c->count].key = key;
    c->table[c->count].off = (uint16_t)off;
    c->count++;
}

static void report_visit(void *ctx, uint16_t key, const uint8_t *value, uint16_t len, uint32_t off) {
    struct foreach_ctx *c = ctx;
    flash_kv_record_t   record = {.offset = off, .key = key, .len = len, .value = value};

    record.status = FLASH_KV_RECORD_OLD;
    for (uint32_t i = 0; i < c->count; i++) {
        if (c->table[i].key == key && c->table[i].off == off) {
            record.status = len ? FLASH_KV_RECORD_LIVE : FLASH_KV_RECORD_DELETED;
            break;
        }
    }
    c->visit(c->user, &record);
}

flash_kv_state_t flash_kv_foreach(flash_kv_visit_fn visit, void *ctx, uint32_t *used) {
    uint32_t               end;
    const flash_kv_state_t state = scan(&end);

    *used = 0;
    if (state != FLASH_KV_VALID && state != FLASH_KV_TORN) return state;

    struct foreach_ctx c = {.table = malloc(MAX_RECORDS * sizeof(struct latest)), .visit = visit, .user = ctx};
    assert(c.table);

    walk(latest_visit, &c, &end);
    walk(report_visit, &c, &end);
    free(c.table);

    *used = end;
    return state;
}
