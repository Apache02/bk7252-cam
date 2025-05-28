#ifndef _HARDWARE_I2C_H
#define _HARDWARE_I2C_H

#include <stddef.h>
#include <stdint.h>


//#define I2C1_DEFAULT_CLK            26000000
//#define I2C1_DEFAULT_DIVIDER        0x16
//#define I2C1_DEFAULT_DIVIDER        130

#ifdef __cplusplus
extern "C" {
#endif

void i2c1_init();

void i2c1_set_baudrate(uint32_t baudrate);

int i2c1_write(uint8_t addr, const uint8_t *data, size_t length, int timeout_ms);

int i2c1_read(uint8_t addr, uint8_t *data, size_t length, int timeout_ms);


// void i2c2_init();
// void i2c2_set_baudrate(uint32_t baudrate);
// int i2c2_write(uint8_t addr, const uint8_t *data, size_t length, int timeout_ms);
// int i2c2_read(uint8_t addr, uint8_t *data, size_t length, int timeout_ms);


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_I2C_H
