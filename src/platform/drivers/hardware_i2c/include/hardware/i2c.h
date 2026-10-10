#ifndef _HARDWARE_I2C_H
#define _HARDWARE_I2C_H

#include <stdint.h>
#include <errno.h> // used for error codes

#ifdef __cplusplus
extern "C" {
#endif

// On the HI704 camera module (I2C1, per board.h) the sensor does not answer
// on the bus at all until the DVP/JPEG block is brought up first (clock gate
// + GPIO27-39 in DCMI mode + JPEG_ENC_EN) — confirmed on hardware. That
// dependency belongs to whoever owns the camera bring-up, not to this driver.
void i2c1_init(uint32_t baud_hz);

// Stops the controller, releases its interrupt and powers its clock gate down.
void i2c1_deinit(void);

// Both block until the transfer's address is ACKed/NACKed and every byte has
// gone back and forth, or until it times out. Return 0 on success, -EBUSY if
// another transfer is already in flight, -EFAULT on a NACK (address or data),
// -ETIMEDOUT on timeout.
int i2c1_write(uint8_t addr7, const uint8_t *data, uint16_t len);

int i2c1_read(uint8_t addr7, uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_I2C_H
