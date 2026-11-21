#include "device/camera_sensor.h"
#include "gc0329_init_table.h"

#define I2C_ADDR (0x31)
#define ID_REG   (0x00)
#define ID_VALUE (0xC0)

// The ID register reads 0 until the analog block is woken up with 0xFC = 0x16.
static bool gc0329_probe(const uint8_t i2c_addr) {
    uint8_t id = 0;

    return camera_bus_write_reg(i2c_addr, 0xFC, 0x16) == 0 && camera_bus_write_reg(i2c_addr, 0xFE, 0x00) == 0 &&
           camera_bus_read_reg(i2c_addr, ID_REG, &id) == 0 && id == ID_VALUE;
}

const camera_sensor_t camera_sensor_gc0329 = {
    .name         = "gc0329",
    .i2c_addr     = I2C_ADDR,
    .width        = 640,
    .height       = 480,
    .mclk         = JPEG_ENCODER_FREQ_24MHZ,
    .yuv_format   = JPEG_ENCODER_YUYV,
    .hsync_invert = false,
    .vsync_invert = false,
    .probe        = gc0329_probe,
    .init_pairs   = gc0329_init_tbl,
    .init_count   = sizeof(gc0329_init_tbl) / sizeof(gc0329_init_tbl[0]),
};
