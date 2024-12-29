#include "string.h"
#include "hardware/flash_regs.h"
#include "hardware/flash.h"


#define min(a, b)               (a<b ? a : b)
#define count_of(x)             (sizeof(x) / sizeof(x[0]))


inline void wait_busy_bit() {
    while (hw_flash->operate_sw.busy);
}

uint32_t flash_id() {
    return hw_flash->JEDEC_ID;
}

void flash_read(uint32_t addr, uint8_t *dst, uint32_t count) {
    uint32_t buf[8];

    wait_busy_bit();

    while (count > 0) {
        hw_flash->operate_sw.addr = addr & ~(0x1f);
        hw_flash->operate_sw.op_type_sw = FLASH_OPCODE_READ;
        hw_flash->operate_sw.op_sw = 1;
        wait_busy_bit();

        addr += 8 * sizeof(uint32_t);

        for (int i = 0; i < count_of(buf); i++) {
            buf[i] = hw_flash->data_flash_sw;
        }

        uint32_t c = min(sizeof(buf), count);
        memcpy(dst, buf, c);
        count -= c;
        dst += c;
    }
}
