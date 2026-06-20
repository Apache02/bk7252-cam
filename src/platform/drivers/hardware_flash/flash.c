#include "soc/flash.h"
#include "hardware/flash.h"
#include <string.h>
#include "platform/init.h"


#define FLASH_READ_WORDS_COUNT (FLASH_READ_BLOCK_SIZE / sizeof(uint32_t))


static inline uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }

static inline void wait_busy_bit() { while (hw_flash->operate_sw.busy); }

/* Single atomic write to operate_sw: sets addr, opcode, wp=1, op_sw=1 together.
 * Avoids read-modify-write races on operate_sw and matches the SDK pattern. */
static inline void trigger(uint32_t addr, FLASH_OPCODE op) {
    hw_write_fields(hw_flash->operate_sw, .addr = addr, .op_type_sw = op, .op_sw = 1, .wp = 1, );
    wait_busy_bit();
}

/* Issue Write Enable (WREN). Required before every SE and PP — the BK7252
 * controller does not auto-issue WREN and suppresses PP/SE without it. */
static void wren(void) { trigger(0, FLASH_OPCODE_WREN); }

void flash_init(void) {
    wait_busy_bit();
    trigger(0, FLASH_OPCODE_RDID);

    hw_write_fields(hw_flash->conf, .clk_conf = 5, .model_sel = 1, .crc_en = 1, );
    hw_flash->sr_data_crc_cnt.m_value = 0xA5;
}
INIT_AT(flash_init, 02);

uint32_t flash_id() { return hw_flash->JEDEC_ID; }

/* Write one 32-byte hardware block to flash using Page Program (PP).
 * aligned_addr must be 32-byte aligned. */
static void write_block(uint32_t aligned_addr, const uint32_t *words) {
    wren();
    for (uint32_t i = 0; i < FLASH_READ_WORDS_COUNT; i++) {
        hw_flash->data_sw_flash = words[i];
    }
    trigger(aligned_addr, FLASH_OPCODE_PP);
}

void flash_read(uint32_t addr, uint8_t *dst, uint32_t count) {
    uint32_t buf[FLASH_READ_WORDS_COUNT];

    wait_busy_bit();

    while (count > 0) {
        const uint32_t aligned_addr = addr & ~FLASH_READ_BLOCK_MASK;
        const uint32_t block_offset = addr - aligned_addr;
        const uint32_t available    = FLASH_READ_BLOCK_SIZE - block_offset;

        trigger(aligned_addr, FLASH_OPCODE_READ);

        for (uint32_t i = 0; i < FLASH_READ_WORDS_COUNT; i++) {
            buf[i] = hw_flash->data_flash_sw;
        }

        const uint32_t c = min_u32(available, count);
        memcpy(dst, ((const uint8_t *)buf) + block_offset, c);

        addr += c;
        dst += c;
        count -= c;
    }
}

uint8_t flash_read_sr1(void) {
    wait_busy_bit();
    trigger(0, FLASH_OPCODE_RDSR);
    return (uint8_t)hw_flash->sr_data_crc_cnt.sr;
}

uint8_t flash_read_sr2(void) {
    wait_busy_bit();
    trigger(0, FLASH_OPCODE_RDSR2);
    return (uint8_t)hw_flash->sr_data_crc_cnt.sr;
}

void flash_write_sr1(uint8_t sr1) {
    wait_busy_bit();
    hw_flash->conf.wrsr_data = sr1;
    wren();
    trigger(0, FLASH_OPCODE_WRSR);
}

void flash_unprotect(void) {
    if (flash_read_sr1() & 0xFCu) {
        flash_write_sr1(0x00u);
    }
}

void flash_erase_sector(uint32_t addr) {
    wait_busy_bit();
    wren();
    trigger(addr & ~FLASH_SECTOR_MASK, FLASH_OPCODE_SE);
}

void flash_write(uint32_t addr, const uint8_t *src, uint32_t count) {
    uint32_t buf[FLASH_READ_WORDS_COUNT];

    wait_busy_bit();

    while (count > 0) {
        const uint32_t aligned_addr = addr & ~FLASH_READ_BLOCK_MASK;
        const uint32_t block_offset = addr - aligned_addr;
        const uint32_t available    = FLASH_READ_BLOCK_SIZE - block_offset;
        const uint32_t c            = min_u32(available, count);

        /* Partial block: preserve the bytes outside the written range. */
        if (block_offset != 0 || c < FLASH_READ_BLOCK_SIZE) {
            flash_read(aligned_addr, (uint8_t *)buf, FLASH_READ_BLOCK_SIZE);
        }

        memcpy((uint8_t *)buf + block_offset, src, c);
        write_block(aligned_addr, buf);

        addr += c;
        src += c;
        count -= c;
    }
}
