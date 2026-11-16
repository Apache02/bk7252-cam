#include "device/camera_sensor.h"
#include "gc0311_init_table.h"

#define I2C_ADDR (0x33)
#define ID_REG   (0xF0)
#define ID_VALUE (0xBB)

// The ID register reads right even before the sensor has been reset and initialized.
static bool gc0311_probe(const uint8_t i2c_addr) {
    uint8_t id = 0;

    return camera_bus_read_reg(i2c_addr, ID_REG, &id) == 0 && id == ID_VALUE;
}

const camera_sensor_t camera_sensor_gc0311 = {
    .name         = "gc0311",
    .i2c_addr     = I2C_ADDR,
    .width        = 640,
    .height       = 480,
    .mclk         = JPEG_ENCODER_FREQ_24MHZ,
    .yuv_format   = JPEG_ENCODER_YUYV,
    .hsync_invert = false,
    .vsync_invert = false,
    .probe        = gc0311_probe,
    .init_pairs   = gc0311_init_tbl,
    .init_count   = sizeof(gc0311_init_tbl) / sizeof(gc0311_init_tbl[0]),
};
