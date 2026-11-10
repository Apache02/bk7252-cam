#include "device/camera_sensor.h"
#include "gc0310_init_table.h"

#define GC0310_I2C_ADDR   (0x21)
#define GC0310_ID_HI_REG  (0xF0)
#define GC0310_ID_LO_REG  (0xF1)
#define GC0310_ID         (0xA310)

static bool gc0310_probe(const uint8_t i2c_addr) {
    uint8_t hi = 0;
    uint8_t lo = 0;
    return camera_bus_read_reg(i2c_addr, GC0310_ID_HI_REG, &hi) == 0
           && camera_bus_read_reg(i2c_addr, GC0310_ID_LO_REG, &lo) == 0
           && ((hi << 8) | lo) == GC0310_ID;
}

const camera_sensor_t camera_sensor_gc0310 = {
    .name         = "gc0310",
    .i2c_addr     = GC0310_I2C_ADDR,
    .width        = 640,
    .height       = 480,
    .mclk         = JPEG_ENCODER_FREQ_24MHZ,
    .yuv_format   = JPEG_ENCODER_YUYV,
    .hsync_invert = false,
    .vsync_invert = false,
    .probe        = gc0310_probe,
    .init_pairs   = gc0310_init_tbl,
    .init_count   = sizeof(gc0310_init_tbl) / sizeof(gc0310_init_tbl[0]),
};
