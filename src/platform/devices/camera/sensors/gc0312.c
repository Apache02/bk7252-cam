#include "device/camera_sensor.h"
#include "gc0312_init_table.h"

#define I2C_ADDR  (0x21)
#define ID_HI_REG (0xF0)
#define ID_LO_REG (0xF1)
#define ID_VALUE  (0xB310)

static bool gc0312_probe(const uint8_t i2c_addr) {
    uint8_t hi = 0;
    uint8_t lo = 0;

    return camera_bus_read_reg(i2c_addr, ID_HI_REG, &hi) == 0 && camera_bus_read_reg(i2c_addr, ID_LO_REG, &lo) == 0 &&
           ((hi << 8) | lo) == ID_VALUE;
}

const camera_sensor_t camera_sensor_gc0312 = {
    .name         = "gc0312",
    .i2c_addr     = I2C_ADDR,
    .width        = 640,
    .height       = 480,
    .mclk         = JPEG_ENCODER_FREQ_24MHZ,
    .yuv_format   = JPEG_ENCODER_YUYV,
    .hsync_invert = false,
    .vsync_invert = false,
    .probe        = gc0312_probe,
    .init_pairs   = gc0312_init_tbl,
    .init_count   = sizeof(gc0312_init_tbl) / sizeof(gc0312_init_tbl[0]),
};
