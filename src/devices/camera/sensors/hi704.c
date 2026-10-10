#include "device/camera_sensor.h"
#include "hi704_init_table.h"

#define HI704_I2C_ADDR (0x30)
#define HI704_ID_REG   (0x04)
#define HI704_ID       (0x96)

static bool hi704_probe(const uint8_t i2c_addr) {
    uint8_t id = 0;
    return camera_bus_read_reg(i2c_addr, HI704_ID_REG, &id) == 0 && id == HI704_ID;
}

const camera_sensor_t camera_sensor_hi704 = {
    .name         = "hi704",
    .i2c_addr     = HI704_I2C_ADDR,
    .width        = 640,
    .height       = 480,
    .mclk         = JPEG_ENCODER_FREQ_24MHZ,
    .yuv_format   = JPEG_ENCODER_YUYV,
    .hsync_invert = false,
    .vsync_invert = false,
    .probe        = hi704_probe,
    .init_pairs   = hi704_init_tbl,
    .init_count   = sizeof(hi704_init_tbl) / sizeof(hi704_init_tbl[0]),
};
