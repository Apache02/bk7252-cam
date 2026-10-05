#ifndef _DEVICE_CAMERA_SENSOR_H
#define _DEVICE_CAMERA_SENSOR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "hardware/jpeg.h"

#ifdef __cplusplus
extern "C" {
#endif

// Everything that differs between camera sensors. One const instance per sensor, listed
// in camera_sensors[] (sensors.c).
typedef struct camera_sensor {
    const char *name;
    uint8_t     i2c_addr;

    // Encoder side. width and height will move to per-mode settings.
    uint16_t                  width;
    uint16_t                  height;
    jpeg_encoder_freq_t       mclk;
    jpeg_encoder_yuv_format_t yuv_format;
    bool                      hsync_invert;
    bool                      vsync_invert;

    // One attempt to recognise the sensor at i2c_addr; the encoder clock is already running.
    bool (*probe)(uint8_t i2c_addr);

    // {register, value} pairs written in order after the sensor is found.
    const uint8_t (*init_pairs)[2];
    size_t init_count;
} camera_sensor_t;

extern const camera_sensor_t *const camera_sensors[];
extern const size_t                 camera_sensor_count;

// Register access on the camera's I2C port, for probe() and sensor code. 8-bit registers.
// Both return 0 or a negative errno.
int camera_bus_read_reg(uint8_t i2c_addr, uint8_t reg, uint8_t *value);
int camera_bus_write_reg(uint8_t i2c_addr, uint8_t reg, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif // _DEVICE_CAMERA_SENSOR_H
