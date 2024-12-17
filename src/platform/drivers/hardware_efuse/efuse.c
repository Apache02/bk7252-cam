#include "hardware/efuse.h"
#include "hardware/sctrl_regs.h"


typedef union {
    uint32_t v;
    struct __attribute__((aligned(4))) __packed {
        uint32_t op_enable: 1;              // [0]
        uint32_t op_direction: 1;           // [1]
        uint32_t reserved_2_7: 6;           // [2:7]
        uint32_t addr: 5;                   // [8:12]
        uint32_t reserved_13_15: 3;         // [13:15]
        uint32_t wr_data: 5;                // [16:20]
        uint32_t reserved_21_31: 11;        // [21:31]
    };
} efuse_ctrl_r;

typedef union {
    uint32_t v;
    struct __attribute__((aligned(4))) __packed {
        uint32_t rd_data: 8;                // [0:7]
        uint32_t is_valid: 1;               // [8]
        uint32_t reserved_9_31: 23;         // [9:31]
    };
} efuse_optr_r;


int efuse_read_byte(uint8_t addr) {
    if (addr > 0x1F) return -1;

    volatile efuse_ctrl_r ctrl;

    ctrl.v = hw_sctrl->efuse_ctrl;
    ctrl.addr = addr;
    ctrl.op_enable = 1;

    hw_sctrl->efuse_ctrl = ctrl.v;

    // wait for read
    int timeout = 1000;
    while (timeout-- > 0) {
        ctrl.v = hw_sctrl->efuse_ctrl;
        if (!ctrl.op_enable) break;
    }
    if (timeout <= 0) return -2;

    volatile efuse_optr_r *optr_ptr = (efuse_optr_r *) &hw_sctrl->efuse_optr;
    if (!optr_ptr->is_valid) return -3;

    return optr_ptr->rd_data;
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
