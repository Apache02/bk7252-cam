#include "string.h"
#include "flash_regs.h"
#include "hardware/flash.h"


#define FLASH_READ_BLOCK            (32u)
#define FLASH_READ_BLOCK_MASK       (FLASH_READ_BLOCK - 1u)
#define FLASH_READ_WORDS_COUNT      (FLASH_READ_BLOCK / sizeof(uint32_t))


static inline uint32_t min_u32(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

static inline void wait_busy_bit() {
    while (hw_flash->operate_sw.busy);
}

uint32_t flash_id() {
    return hw_flash->JEDEC_ID;
}

void flash_read(uint32_t addr, uint8_t *dst, uint32_t count) {
    uint32_t buf[FLASH_READ_WORDS_COUNT];

    wait_busy_bit();

    while (count > 0) {
        const uint32_t aligned_addr = addr & ~FLASH_READ_BLOCK_MASK;
        const uint32_t block_offset = addr - aligned_addr;
        const uint32_t available = FLASH_READ_BLOCK - block_offset;

        hw_flash->operate_sw.addr = aligned_addr;
        hw_flash->operate_sw.op_type_sw = FLASH_OPCODE_READ;
        hw_flash->operate_sw.op_sw = 1;
        wait_busy_bit();

        for (uint32_t i = 0; i < FLASH_READ_WORDS_COUNT; i++) {
            buf[i] = hw_flash->data_flash_sw;
        }

        const uint32_t c = min_u32(available, count);
        memcpy(dst, ((const uint8_t *) buf) + block_offset, c);

        addr += c;
        dst += c;
        count -= c;
    }
}
