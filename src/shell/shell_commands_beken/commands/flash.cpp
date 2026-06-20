#include "shell/commands_beken.h"
#include "shell/Parser.h"
#include "hardware/flash.h"
#include "utils/busy_wait.h"
#include "utils/crc32.h"
#include <stdio.h>
#include <string.h>

#define CHUNK       (256u)
#define SECTOR_SIZE FLASH_SECTOR_SIZE

#define DUMP_DEFAULT_SIZE (1 * CHUNK)
#define DUMP_MAX_SIZE     (16 * CHUNK)

#define ACK            0x06
#define ACK_TIMEOUT_MS 5000
#define POLL_INTERVAL  10

// validations

static bool valid_size(uint32_t size) { return size > 0 && size <= DUMP_MAX_SIZE; }

static bool inline valid_ram(uint32_t addr) {
    return (addr >= 0x00400000 && addr < 0x00400000 + 0x00040000) ||
           (addr >= 0x00900000 && addr < 0x00900000 + 0x00040000);
}

static bool inline valid_checksum(uint32_t addr1, uint32_t addr2, uint32_t size, uint32_t checksum) {
    return addr1 + addr2 + size == checksum;
}

// helpers

static void print_dump(const uint8_t *buf, uint32_t count) {
    for (size_t i = 0, offset = 0; i < count; ++i) {
        printf("%02x ", buf[i]);

        offset += sizeof(uint8_t);
        if (offset % 16 == 0) {
            printf("\r\n");
        }
    }
}

static uint32_t flash_crc32(uint32_t flash_addr, uint32_t size) {
    uint8_t  buf[CHUNK];
    uint32_t crc = crc32_init();
    for (uint32_t off = 0; off < size; off += CHUNK) {
        uint32_t n = size - off;
        if (n > CHUNK) n = CHUNK;
        flash_read(flash_addr + off, buf, n);
        crc = crc32_update(crc, buf, n);
    }
    return crc32_final(crc);
}

static bool has_ack(uint32_t timeout_ms) {
    int  countdown = timeout_ms / POLL_INTERVAL;
    bool ack       = false;
    while (countdown > 0 && !ack) {
        busy_wait_ms(POLL_INTERVAL);
        int c = getchar();
        if (c == ACK) {
            ack = true;
        } else if (c >= 0) {
            countdown = timeout_ms / POLL_INTERVAL;
        } else {
            countdown--;
        }
    }

    return ack;
}

// commands

int command_flash_crc32(int argc, const char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <addr> <size>\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));

    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }

    printf("CRC32 0x%08lx\r\n", flash_crc32(addr, size));
    return 0;
}

int command_flash_dump(int argc, const char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Usage: %s <addr> [size]\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size = argc == 3 ? static_cast<uint32_t>(take_int(argv[2]).ok_or(DUMP_DEFAULT_SIZE)) : DUMP_DEFAULT_SIZE;

    if (!valid_size(size)) {
        printf("Invalid size\r\n");
        return 1;
    }

    printf("addr 0x%08lx\r\n", addr);

    uint8_t buf[CHUNK];
    for (uint32_t off = 0; off < size; off += CHUNK) {
        uint32_t n = size - off;
        if (n > CHUNK) n = CHUNK;

        flash_read(addr + off, buf, n);
        print_dump(buf, n);
    }
    printf("\r\n");

    return 0;
}

int command_flash_read_binary(int argc, const char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <addr> <size>\r\n", argv[0]);
        return 1;
    }

    uint32_t addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t size = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));

    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }

    printf("Send ACK (0x06) to start binary transfer or wait to abort\r\n");

    if (!has_ack(ACK_TIMEOUT_MS)) {
        printf("Aborted\r\n");
        return 0;
    }

    uint8_t  buf[CHUNK];
    uint32_t crc = crc32_init();

    for (uint32_t off = 0; off < size; off += CHUNK) {
        uint32_t n = size - off;
        if (n > CHUNK) n = CHUNK;
        flash_read(addr + off, buf, n);
        for (uint32_t i = 0; i < n; i++) putchar(buf[i]);
        crc = crc32_update(crc, buf, n);
    }

    crc = crc32_final(crc);
    putchar(static_cast<uint8_t>(crc & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 8) & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 16) & 0xFF));
    putchar(static_cast<uint8_t>((crc >> 24) & 0xFF));

    return 0;
}

int command_flash_write(int argc, const char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <ram_addr> <flash_addr> <size> <checksum>\r\n", argv[0]);
        printf("  checksum = ram_addr + flash_addr + size\r\n");
        return 1;
    }

    uint32_t from_addr = static_cast<uint32_t>(take_int(argv[1]).ok_or(0));
    uint32_t to_addr   = static_cast<uint32_t>(take_int(argv[2]).ok_or(0));
    uint32_t size      = static_cast<uint32_t>(take_int(argv[3]).ok_or(0));

    if (!valid_checksum(from_addr, to_addr, size, static_cast<uint32_t>(take_int(argv[4]).ok_or(0)))) {
        printf("Invalid checksum\r\n");
        return 1;
    }

    if (!valid_ram(from_addr)) {
        printf("Invalid argument ram_addr\r\n");
        return 1;
    }

    if (size == 0) {
        printf("Invalid size\r\n");
        return 1;
    }

    uint32_t first_sector = (to_addr / SECTOR_SIZE) * SECTOR_SIZE;
    uint32_t last_sector  = ((to_addr + size - 1) / SECTOR_SIZE) * SECTOR_SIZE;
    for (uint32_t s = first_sector; s <= last_sector; s += SECTOR_SIZE) {
        flash_erase_sector(s);
        printf(".");
    }
    printf("\r\n");

    flash_write(to_addr, reinterpret_cast<const uint8_t *>(from_addr), size);

    printf("CRC32 0x%08lx\r\n", flash_crc32(to_addr, size));

    return 0;
}

typedef union {
    uint8_t v;
    struct {
        uint8_t wip : 1;  // [0] Write In Progress (read-only, hardware-cleared)
        uint8_t wel : 1;  // [1] Write Enable Latch (set by WREN, cleared after write/erase)
        uint8_t bp : 3;   // [4:2] Block Protect
        uint8_t tb : 1;   // [5] Top/Bottom protect (0=top, 1=bottom)
        uint8_t sec : 1;  // [6] Sector/Block select (0=64KB blocks, 1=4KB sectors)
        uint8_t srp0 : 1; // [7] Status Register Protect 0 (hardware WP# pin enable)
    };
} flash_sr1_t;

typedef union {
    uint8_t v;
    struct {
        uint8_t srp1 : 1; // [0] Status Register Protect 1
        uint8_t qe : 1;   // [1] Quad Enable
        uint8_t r : 1;    // [2] Reserved
        uint8_t lb1 : 1;  // [3] Security Register Lock 1
        uint8_t lb2 : 1;  // [4] Security Register Lock 2
        uint8_t lb3 : 1;  // [5] Security Register Lock 3
        uint8_t cmp : 1;  // [6] Complement Protect
        uint8_t sus : 1;  // [7] Erase/Write Suspend
    };
} flash_sr2_t;

static void print_flash_info() {
    uint32_t    fid = flash_id();
    flash_sr1_t sr1;
    flash_sr2_t sr2;
    sr1.v = flash_read_sr1();
    sr2.v = flash_read_sr2();

    int size_mb = (1u << (fid & 0xFF)) >> 20;

    printf("flash id: 0x%08lx   %d MB   %s\r\n", fid, size_mb, sr1.bp ? "[PROTECTED]" : "");
    if (sr1.bp) printf("    Protection: %d/%d %s\r\n", sr1.bp, 7, sr1.tb ? "bottom" : "top");
    printf("    SR1: 0x%02x   WIP=%d  WEL=%d  BP=%d  TB=%d  SEC=%d  SRP0=%d\r\n", sr1.v, sr1.wip, sr1.wel, sr1.bp,
           sr1.tb, sr1.sec, sr1.srp0);
    printf("    SR2: 0x%02x   SRP1=%d  QE=%d  CMP=%d  SUS=%d\r\n", sr2.v, sr2.srp1, sr2.qe, sr2.cmp, sr2.sus);
}

int command_flash(int argc, const char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <id|unprotect>\r\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "id") == 0) {
        print_flash_info();
        return 0;
    }

    if (strcmp(argv[1], "unprotect") == 0) {
        flash_unprotect();
        printf("done\r\n\r\n");
        print_flash_info();
        return 0;
    }

    printf("Unknown action: %s\r\n", argv[1]);
    return 1;
}
