#include "hardware/efuse.h"
#include "efuse_regs.h"


int efuse_read_byte(uint8_t addr) {
    if (addr > 0x1F) return -1;

    hw_efuse->ctrl.addr = addr;
    hw_efuse->ctrl.dir = 0;
    hw_efuse->ctrl.en = 1;

    // wait for read
    for (int timeout = 1000; hw_efuse->ctrl.en; timeout--) {
        if (timeout <= 0) return -2;
    }

    if (!hw_efuse->optr.is_valid) return -3;

    return hw_efuse->optr.rd_data;
}

static int efuse_read_block(uint8_t *dst, uint8_t offset, uint8_t length) {
    for (int i = 0; i < length; i++) {
        int byte = efuse_read_byte(offset + i);
        if (byte < 0) return byte;
        dst[i] = byte;
    }

    return 0;
}

int efuse_read_encrypt_word(uint8_t *buf) {
    return efuse_read_block(buf, EFUSE_ENCRYPT_WORD_OFFSET, EFUSE_ENCRYPT_WORD_LEN);
}

int efuse_read_charge_cal(uint8_t *buf) {
    return efuse_read_block(buf, EFUSE_CHARGE_CAL_OFFSET, EFUSE_CHARGE_CAL_LEN);
}

int efuse_read_uid(uint8_t *uid) {
    return efuse_read_block(uid, EFUSE_UID_OFFSET, EFUSE_UID_LEN);
}

int efuse_read_mac(uint8_t *mac) {
    return efuse_read_block(mac, EFUSE_MAC_OFFSET, EFUSE_MAC_LEN);
}

int efuse_read_user_area(uint8_t *buf) {
    int result = efuse_read_byte(EFUSE_USER_AREA_OFFSET);
    if (result < 0) return result;
    *buf = result;
    return 0;
}

int efuse_read_ctrl(uint8_t *buf) {
    int result = efuse_read_byte(EFUSE_CTRL_OFFSET);
    if (result < 0) return result;
    *buf = result;
    return 0;
}
