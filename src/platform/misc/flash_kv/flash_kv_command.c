#include "shell/commands_flash_kv.h"
#include "hardware/flash.h"
#include "platform/flash_kv.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HEADER_SIZE   8u
#define VALUE_PREVIEW 16u

struct summary {
    uint32_t records;
    uint32_t live;
};

static const char *state_name(flash_kv_state_t state) {
    switch (state) {
    case FLASH_KV_BLANK: return "blank";
    case FLASH_KV_FOREIGN: return "foreign";
    case FLASH_KV_VALID: return "valid";
    case FLASH_KV_TORN: return "torn";
    }
    return "unknown";
}

static const char *status_name(flash_kv_record_status_t status) {
    switch (status) {
    case FLASH_KV_RECORD_LIVE: return "live";
    case FLASH_KV_RECORD_OLD: return "old";
    case FLASH_KV_RECORD_DELETED: return "deleted";
    }
    return "unknown";
}

// The first bytes in hex, then in quotes if they are all printable.
static void print_value(const uint8_t *value, uint16_t len) {
    const uint32_t shown = len < VALUE_PREVIEW ? len : VALUE_PREVIEW;
    bool           text  = len > 0;

    for (uint32_t i = 0; i < shown; i++) {
        printf("%02x ", value[i]);
        if (value[i] < 0x20 || value[i] > 0x7E) text = false;
    }
    if (len > shown) printf("... ");

    if (text) {
        printf(" \"");
        for (uint32_t i = 0; i < shown; i++) printf("%c", value[i]);
        printf(len > shown ? "...\"" : "\"");
    }
}

static void print_record(void *ctx, const flash_kv_record_t *record) {
    struct summary *summary = ctx;

    summary->records++;
    if (record->status == FLASH_KV_RECORD_LIVE) summary->live++;

    printf("%04lx    0x%04x  %4u  %-8s ", (unsigned long)record->offset, record->key, (unsigned)record->len,
           status_name(record->status));
    print_value(record->value, record->len);
    printf("\r\n");
}

static void print_header(void) {
    uint8_t header[HEADER_SIZE];
    flash_read(FLASH_KV_ADDR, header, sizeof(header));

    printf("header: ");
    for (uint32_t i = 0; i < sizeof(header); i++) printf("%02x ", header[i]);
    printf("\r\n");
}

static void list_records(void) {
    const flash_kv_state_t state = flash_kv_state();

    print_header();
    printf("state: %s\r\n", state_name(state));

    if (state == FLASH_KV_BLANK) {
        printf("the sector is empty\r\n");
        return;
    }
    if (state == FLASH_KV_FOREIGN) {
        printf("the sector holds data that is not ours, the first write erases it\r\n");
        return;
    }

    struct summary summary = {0};
    uint32_t       used;

    printf("\r\noffset  key     len  status   value\r\n");
    flash_kv_foreach(print_record, &summary, &used);

    printf("\r\nrecords: %lu, live: %lu\r\n", (unsigned long)summary.records, (unsigned long)summary.live);
    printf("used: %lu of %lu bytes\r\n", (unsigned long)used, (unsigned long)FLASH_KV_SIZE);
    if (state == FLASH_KV_TORN) {
        printf("a broken record follows at %04lx, the next write compacts\r\n", (unsigned long)used);
    }
}

static bool sector_is_blank(void) {
    uint8_t block[FLASH_READ_BLOCK_SIZE];

    for (uint32_t off = 0; off < FLASH_KV_SIZE; off += sizeof(block)) {
        flash_read(FLASH_KV_ADDR + off, block, sizeof(block));
        for (uint32_t i = 0; i < sizeof(block); i++) {
            if (block[i] != 0xFF) return false;
        }
    }
    return true;
}

static int erase_sector(void) {
    flash_erase_sector(FLASH_KV_ADDR);

    if (!sector_is_blank()) {
        printf("the sector is not blank after the erase, is the flash protected?\r\n");
        return 1;
    }
    printf("erased\r\n");
    return 0;
}

int command_flash_kv(int argc, const char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <dump|erase>\r\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "dump") == 0) {
        list_records();
        return 0;
    }

    if (strcmp(argv[1], "erase") == 0) return erase_sector();

    printf("Unknown action: %s\r\n", argv[1]);
    return 1;
}
