#ifndef _DEVICE_CAMERA_I2C_H
#define _DEVICE_CAMERA_I2C_H

#include "board.h"
#include "hardware/i2c.h"


_Static_assert(__builtin_strcmp(CAMERA_I2C, "i2c1") == 0 || __builtin_strcmp(CAMERA_I2C, "i2c2") == 0,
               "CAMERA_I2C must be \"i2c1\" or \"i2c2\"");

#define IS_I2C1 (__builtin_strcmp(CAMERA_I2C, "i2c1") == 0)

static inline void i2c_init(uint32_t baud) {
    IS_I2C1 ? i2c1_init(baud) : i2c2_init(baud);
}

static inline void i2c_deinit(void) {
    IS_I2C1 ? i2c1_deinit() : i2c2_deinit();
}

static inline int i2c_write(uint8_t addr7, const uint8_t *data, uint16_t len) {
    return IS_I2C1 ? i2c1_write(addr7, data, len) : i2c2_write(addr7, data, len);
}

static inline int i2c_read(uint8_t addr7, uint8_t *data, uint16_t len) {
    return IS_I2C1 ? i2c1_read(addr7, data, len) : i2c2_read(addr7, data, len);
}

#endif // _DEVICE_CAMERA_I2C_H
