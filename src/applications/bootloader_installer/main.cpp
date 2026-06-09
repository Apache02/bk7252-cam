#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "platform/stdio.h"
#include "hardware/flash.h"
#include "hardware/wdt.h"
#include "utils/busy_wait.h"

extern "C" {
extern const uint8_t _bootloader_payload_start[];
extern const uint8_t _bootloader_payload_end[];
}

static const uint8_t  *const payload      = _bootloader_payload_start;
static const uint32_t        payload_size =
    static_cast<uint32_t>(_bootloader_payload_end - _bootloader_payload_start);

static constexpr uint32_t FLASH_ADDR  = 0x000000u;
static constexpr uint32_t SECTOR_SIZE = 4096u;
static constexpr uint32_t CHUNK       = 256u;

static bool flash_matches() {
    uint8_t buf[CHUNK];
    for (uint32_t off = 0; off < payload_size; off += CHUNK) {
        uint32_t n = payload_size - off;
        if (n > CHUNK) n = CHUNK;
        flash_read(FLASH_ADDR + off, buf, n);
        if (memcmp(buf, payload + off, n) != 0) return false;
    }
    return true;
}

static uint32_t find_mismatch() {
    uint8_t buf[CHUNK];
    for (uint32_t off = 0; off < payload_size; off += CHUNK) {
        uint32_t n = payload_size - off;
        if (n > CHUNK) n = CHUNK;
        flash_read(FLASH_ADDR + off, buf, n);
        for (uint32_t i = 0; i < n; i++) {
            if (buf[i] != payload[off + i]) return FLASH_ADDR + off + i;
        }
    }
    return UINT32_MAX;
}

int main() {
    wdt_down();
    platform_stdio_init();
    setvbuf(stdout, NULL, _IONBF, 0);
    busy_wait_ms(20);

    printf("Bootloader installer\r\n");
    printf("Embedded binary: %lu bytes\r\n", static_cast<unsigned long>(payload_size));
    busy_wait_ms(1000);

    if (flash_matches()) {
        printf("Already installed.\r\n");
        return 0;
    }

    printf("Installing...\r\n");

    flash_unprotect();

    uint32_t sectors = (payload_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; i++) {
        flash_erase_sector(FLASH_ADDR + i * SECTOR_SIZE);
        printf(".");
    }
    printf("\r\n");

    flash_write(FLASH_ADDR, payload, payload_size);

    uint32_t mismatch = find_mismatch();
    if (mismatch == UINT32_MAX) {
        printf("OK\r\n");
    } else {
        printf("FAILED (addr 0x%06lx)\r\n", static_cast<unsigned long>(mismatch));
    }

    return 0;
}
