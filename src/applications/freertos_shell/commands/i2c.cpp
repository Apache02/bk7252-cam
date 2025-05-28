#include <stdio.h>
#include "shell/Console.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/icu.h"

#define DEBUG_NAME "I2C1"

#include "shell/console_debug.h"

#define count_of(x)     (sizeof(x) / sizeof(x[0]))


static bool reserved_addr(uint8_t addr) {
//    return addr != 0x3c && addr != 0x3b && addr != 0x30;
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

static void enable_jpeg_encoder_hack() {
    // reset
    *((volatile uint32_t *)(0x0080A000)) = 0;
    // clear int
    *((volatile uint32_t *)(0x0080A018)) = *((volatile uint32_t *)(0x0080A018));
    // enable
    *((volatile uint32_t *)(0x0080A004)) = *((volatile uint32_t *)(0x0080A004)) | (1 << 4);

    hw_icu->peri_clk_pwd.jpeg_encoder = 0;

    gpio_config_function(GPIO_FUNC_DCMI);

}

void command_i2c1_scan(Console &c) {
    i2c1_init();
    enable_jpeg_encoder_hack();

    static const uint8_t tx[1] = {0};

    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\r\n");
    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        if (reserved_addr(addr)) {
            printf("_");
        } else {
            int count = i2c1_write(addr, tx, sizeof(tx), 20);

            if (count < 0) {
                printf(".");
            } else if (count == sizeof(tx)) {
                printf("@");
            } else {
                printf("%d", count);
            }
        }

        printf(addr % 16 == 15 ? "\r\n" : "  ");
    }
}


#define SSD1306_ADDR 0x3C

#define SSD1306_CMD  0x00
#define SSD1306_DATA 0x40

static const uint8_t ssd1306_init_tbl[][2] = {
        // Display OFF
        {SSD1306_CMD, 0xAE},
        // Set MUX Ratio (0x1F = 31 => 32 lines)
        {SSD1306_CMD, 0xA8},
        {SSD1306_CMD, 0x1F},
        // Set Display Offset
        {SSD1306_CMD, 0xD3},
        {SSD1306_CMD, 0x00},
        // Set Display Start Line
        {SSD1306_CMD, 0x40},
        // Segment re-map
        {SSD1306_CMD, 0xA1},
        // COM Output Scan Direction
        {SSD1306_CMD, 0xC8},
        // Set COM Pins hardware config
        {SSD1306_CMD, 0xDA},
        {SSD1306_CMD, 0x02},
        // Set Contrast Control
        {SSD1306_CMD, 0x81},
        {SSD1306_CMD, 0x7F},
        // Entire Display ON (resume RAM)
        {SSD1306_CMD, 0xA4},
        // Normal display
        {SSD1306_CMD, 0xA6},
        // Set Memory Addressing Mode = Horizontal
        {SSD1306_CMD, 0x20},
        {SSD1306_CMD, 0x00},
        // Display ON
        {SSD1306_CMD, 0xAF},
};

void command_i2c1_test(Console &c) {
    i2c1_init();
    enable_jpeg_encoder_hack();

    // init display
    LOG_I("init display");
    for (unsigned int i = 0; i < count_of(ssd1306_init_tbl); i++) {
        int count = i2c1_write(SSD1306_ADDR, ssd1306_init_tbl[i], sizeof(ssd1306_init_tbl[0]), 100);
        LOG_D("i2c1_write(0x%02x, 0x%02x): %d", ssd1306_init_tbl[i][0], ssd1306_init_tbl[i][1], count);
    }

    // set cursor
    LOG_I("set cursor");
    uint8_t page = 0;
    uint8_t column = 0;
    static const uint8_t ssd1306_set_cursor_tbl[][2] = {
            {SSD1306_CMD, static_cast<uint8_t>(0xB0 | page)},
            {SSD1306_CMD, static_cast<uint8_t>(0x00 | (column & 0x0F))},
            {SSD1306_CMD, static_cast<uint8_t>(0x10 | ((column >> 4) & 0x0F))},
    };

    for (unsigned int i = 0; i < count_of(ssd1306_set_cursor_tbl); i++) {
        int count = i2c1_write(SSD1306_ADDR, ssd1306_set_cursor_tbl[i], sizeof(ssd1306_set_cursor_tbl[0]), 100);
        LOG_D("i2c1_write(0x%02x, 0x%02x): %d", ssd1306_set_cursor_tbl[i][0], ssd1306_set_cursor_tbl[i][1], count);
    }

    // draw
    LOG_I("draw");
    static const uint8_t ssd1306_draw_example[2] = {SSD1306_DATA, 0xAF};
    int count = i2c1_write(SSD1306_ADDR, ssd1306_draw_example, sizeof(ssd1306_draw_example), 100);
    LOG_D("i2c1_write(0x%02x, 0x%02x): %d", ssd1306_draw_example[0], ssd1306_draw_example[1], count);

    LOG_I("done.");
}
