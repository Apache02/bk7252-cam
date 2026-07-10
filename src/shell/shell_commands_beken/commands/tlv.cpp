#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "platform/fal.h"
#include "platform/tlv.h"
#include "platform/manual_cal.h"
#include "hardware/flash.h"
#include <stdio.h>
#include <string.h>

struct part_read_ctx {
    const fal_partition_t *part;
};

static int read_from_part(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    auto *pc = static_cast<part_read_ctx *>(ctx);
    return fal_read(pc->part, offset, buf, len);
}

static int read_from_flash(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    (void)ctx;
    flash_read(offset, buf, len);
    return 0;
}

static int read_from_xip(void *ctx, uint32_t offset, uint8_t *buf, uint32_t len) {
    (void)ctx;
    memcpy(buf, (const void *)offset, len);
    return 0;
}

struct visit_ctx {
    tlv_read_fn read;
    void       *read_ctx;
};

static void print_hex(const uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        printf(i + 1 < len ? "%02x " : "%02x", buf[i]);
    }
}

static void print_entry(const tlv_entry_t *entry, void *ctx) {
    auto *vc     = static_cast<visit_ctx *>(ctx);
    int   indent = static_cast<int>(entry->depth) * 2;

    if (tlv_manual_cal_is_container(entry->type)) {
        const char *name = tlv_manual_cal_name(entry->type);
        printf("%*s%s (container, len=%lu)\r\n", indent, "", name ? name : "?", (unsigned long)entry->len);
        return;
    }

    uint8_t  payload[TLV_MANUAL_CAL_MAX_PAYLOAD];
    uint32_t n = entry->len < TLV_MANUAL_CAL_MAX_PAYLOAD ? entry->len : TLV_MANUAL_CAL_MAX_PAYLOAD;
    if (vc->read(vc->read_ctx, entry->payload_offset, payload, n) != 0) {
        printf("%*s0x%08lx <read error>\r\n", indent, "", (unsigned long)entry->type);
        return;
    }

    char text[128];
    if (tlv_manual_cal_decode(entry->type, payload, n, text, sizeof(text))) {
        const char *name = tlv_manual_cal_name(entry->type);
        printf("%*s%-22s = %s\r\n", indent, "", name ? name : "?", text);
    } else {
        printf("%*s0x%08lx[%lu] = ", indent, "", (unsigned long)entry->type, (unsigned long)entry->len);
        print_hex(payload, n);
        if (entry->len > n) printf(" ...(truncated)");
        printf("\r\n");
    }
}

int command_tlv(int argc, const char *argv[]) {
    if (argc < 3) {
        printf("Usage: %s part <name>\r\n", argv[0]);
        printf("       %s flash <addr> [size]\r\n", argv[0]);
        printf("       %s addr <addr> [size]\r\n", argv[0]);
        return 1;
    }

    part_read_ctx pc{};
    visit_ctx     vc{};
    uint32_t      start, bound;

    if (strcmp(argv[1], "part") == 0) {
        pc.part = fal_find_partition(argv[2]);
        if (!pc.part) {
            printf("partition '%s' not found\r\n", argv[2]);
            return 1;
        }
        // clang-format off
        printf("partition '%s' @ %s +0x%08lx, size %lu\r\n",
            pc.part->name,
            pc.part->flash_name,
            static_cast<unsigned long>(pc.part->offset),
            static_cast<unsigned long>(pc.part->len)
        );
        // clang-format on
        vc.read     = read_from_part;
        vc.read_ctx = &pc;
        start       = 0;
        bound       = pc.part->len;
    } else if (strcmp(argv[1], "flash") == 0) {
        start = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
        bound = argc >= 4 ? static_cast<uint32_t>(take_int(argv[3]).ok_or(FLASH_SECTOR_SIZE)) : FLASH_SECTOR_SIZE;
        printf("flash 0x%08lx, size %lu\r\n", static_cast<unsigned long>(start), static_cast<unsigned long>(bound));
        vc.read     = read_from_flash;
        vc.read_ctx = nullptr;
    } else if (strcmp(argv[1], "addr") == 0) {
        start = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
        bound = argc >= 4 ? static_cast<uint32_t>(take_int(argv[3]).ok_or(FLASH_SECTOR_SIZE)) : FLASH_SECTOR_SIZE;
        printf("addr 0x%08lx, size %lu\r\n", static_cast<unsigned long>(start), static_cast<unsigned long>(bound));
        vc.read     = read_from_xip;
        vc.read_ctx = nullptr;
    } else {
        printf("Unknown action: %s\r\n", argv[1]);
        return 1;
    }

    uint32_t total_len = 0;
    int      rc        = tlv_read_header(vc.read, vc.read_ctx, start, bound, &total_len);

    if (rc == TLV_ERR_NO_MAGIC) {
        printf("no TLV header at 0x%08lx\r\n", (unsigned long)start);
        return 1;
    }
    if (rc == TLV_ERR_TOO_LARGE) {
        printf("TLV total_len exceeds bound (%lu)\r\n", (unsigned long)bound);
        return 1;
    }
    if (rc != TLV_OK) {
        printf("TLV header read failed (%d)\r\n", rc);
        return 1;
    }

    printf("TLV magic ok, total_len=%lu\r\n\r\n", (unsigned long)total_len);

    rc = tlv_walk(vc.read, vc.read_ctx, start + 8, total_len, tlv_manual_cal_is_container, print_entry, &vc);
    if (rc != TLV_OK) {
        printf("\r\nTLV walk aborted (%d)\r\n", rc);
        return 1;
    }

    printf("\r\n");
    return 0;
}
