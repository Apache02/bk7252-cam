#ifndef _HARDWARE_EFUSE_H
#define _HARDWARE_EFUSE_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/cdefs.h>


// efuse structure
//  0 | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 | ENCRYPT_WORD
// 16 | 00 00 00 00                                     | CHARGE_CAL
// 20 | 00 00 00 00                                     | UID_ADDR
// 24 | 00 00 00 00 00 00                               | MAC_ADDRESS
// 30 | 00                                              | USER_AREA
// 31 | 00                                              | CTRL


#define EFUSE_ENCRYPT_WORD_OFFSET                       (0)
#define EFUSE_ENCRYPT_WORD_LEN                          (16)

#define EFUSE_CHARGE_CAL_OFFSET                         (16)
#define EFUSE_CHARGE_CAL_LEN                            (4)

#define EFUSE_UID_OFFSET                                (20)
#define EFUSE_UID_LEN                                   (4)

#define EFUSE_MAC_OFFSET                                (24)
#define EFUSE_MAC_LEN                                   (6)

#define EFUSE_USER_AREA_OFFSET                          (30)
#define EFUSE_USER_AREA_LEN                             (1)

#define EFUSE_CTRL_OFFSET                               (31)
#define EFUSE_CTRL_LEN                                  (1)


typedef struct __packed {
    uint8_t disable_write: 1;
    uint8_t mac_disable_write: 1;
    uint8_t uid_disable_write: 1;
    uint8_t encrypt_disable_write: 1;
    uint8_t encrypt_disable_read: 1;
    uint8_t encrypt_enable: 1;
    uint8_t flash_download_disable: 1;
    uint8_t jtag_disable: 1;
} efuse_ctrl_byte_t;


#ifdef __cplusplus
extern "C" {
#endif

int efuse_read_byte(uint8_t addr);

int efuse_read_encrypt_word(uint8_t *buf);

int efuse_read_charge_cal(uint8_t *buf);

int efuse_read_uid(uint8_t *uid);

int efuse_read_mac(uint8_t *mac);

int efuse_read_user_area(uint8_t *buf);

int efuse_read_ctrl(uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_EFUSE_H
