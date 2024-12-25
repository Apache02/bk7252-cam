#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/efuse.h"


void command_efuse(__unused Console &c) {
    uint8_t buf[32] = {0};

    if (efuse_read_encrypt_word(buf) == 0) {
        printf(
                "efuse_read_encrypt_word: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\r\n",
                buf[0], buf[1], buf[2], buf[3],
                buf[4], buf[5], buf[6], buf[7],
                buf[8], buf[9], buf[10], buf[11],
                buf[12], buf[13], buf[14], buf[15]
        );
    } else {
        printf("efuse_read_encrypt_word: error\r\n");
    }

    if (efuse_read_charge_cal(buf) == 0) {
        printf(
                "efuse_read_charge_cal: %02x %02x %02x %02x\r\n",
                buf[0], buf[1], buf[2], buf[3]
        );
    } else {
        printf("efuse_read_charge_cal: error\r\n");
    }

    if (efuse_read_uid(buf) == 0) {
        printf(
                "efuse_read_uid: %02x %02x %02x %02x\r\n",
                buf[0], buf[1], buf[2], buf[3]
        );
    } else {
        printf("efuse_read_uid: error\r\n");
    }

    if (efuse_read_mac(buf) == 0) {
        printf(
                "efuse_read_mac: %02x %02x %02x %02x %02x %02x\r\n",
                buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]
        );
    } else {
        printf("efuse_read_mac: error\r\n");
    }

    if (efuse_read_user_area(&buf[0]) == 0) {
        printf("efuse_read_user_area: %02x\r\n", buf[0]);
    } else {
        printf("efuse_read_user_area: error\r\n");
    }

    uint8_t efuse_ctrl;
    if (efuse_read_ctrl(&efuse_ctrl) == 0) {
        printf("efuse_read_ctrl: %02x\r\n", efuse_ctrl);

        printf("    disable_write: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->disable_write);
        printf("    mac_disable_write: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->mac_disable_write);
        printf("    uid_disable_write: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->uid_disable_write);
        printf("    encrypt_disable_write: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->encrypt_disable_write);
        printf("    encrypt_disable_read: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->encrypt_disable_read);
        printf("    encrypt_enable: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->encrypt_enable);
        printf("    flash_download_disable: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->flash_download_disable);
        printf("    jtag_disable: %d\r\n", ((efuse_ctrl_byte_t *)&efuse_ctrl)->jtag_disable);
    } else {
        printf("efuse_read_ctrl: error\r\n");
    }
}