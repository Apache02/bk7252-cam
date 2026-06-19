#pragma once

#include "platform/soc.h"


#define JPEG_BASE_ADDR              (0x0080A000)

typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t reserved_0_1:    2;   // [1:0]
            uint32_t start_frm_int:   1;   // [2]     start-of-frame interrupt enable
            uint32_t end_frm_int:     1;   // [3]     end-of-frame interrupt enable
            uint32_t div:             2;   // [5:4]   MCLK divider: 0=24 MHz, 1=16 MHz, 2=12 MHz, 3=24 MHz
            uint32_t reserved_6_31:  26;   // [31:6]
        };
    } ctrl0;

    union {
        uint32_t v;
        struct {
            uint32_t reserved_0:      1;   // [0]
            uint32_t video_byte_rev:  1;   // [1]     byte reversal of incoming video data
            uint32_t yuv_fmt_sel:     2;   // [3:2]   YUV byte order: 0=YUYV, 1=UYVY, 2=YYUV, 3=UVYY
            uint32_t enc_en:          1;   // [4]     JPEG encoder enable
            uint32_t reserved_5_7:    3;   // [7:5]
            uint32_t x_pixel:         8;   // [15:8]  horizontal pixel count
            uint32_t enc_size:        1;   // [16]    append 4-byte size field to end of each JPEG output
            uint32_t bit_rate_ctrl:   1;   // [17]    automatic bit-rate control enable
            uint32_t bit_rate_step:   3;   // [20:18] bit-rate adjustment step 0–7 (BK7221U: 3 bits; BK7271/BK7236: 2 bits)
            uint32_t hsync_rev:       1;   // [21]    HSYNC polarity invert
            uint32_t vsync_rev:       1;   // [22]    VSYNC polarity invert
            uint32_t reserved_23:     1;   // [23]
            uint32_t y_pixel:         8;   // [31:24] vertical pixel count
        };
    } ctrl1;

    uint32_t target_byte_h;              // max bytes per frame for bit-rate control
    uint32_t target_byte_l;              // min bytes per frame for bit-rate control

    uint32_t reserved_0x10;              // JPEG_REG4 not defined in SDK

    uint32_t rx_fifo_data;               // encoded JPEG output FIFO; DMA source port, read-only

    union {
        uint32_t v;
        struct {
            uint32_t start_frm_int_status:  1;   // [0]    start-of-frame interrupt; W1C
            uint32_t end_frm_int_status:    1;   // [1]    end-of-frame interrupt; W1C
            uint32_t reserved_2_31:        30;   // [31:2]
        };
    } status;

    uint32_t byte_cnt_pfrm;             // byte count of last encoded frame; read-only

    uint32_t reg8;                      // (NOT INVESTIGATED; SDK defines only JPEFG_READ bit 0, typo for JPEG_READ; never used in SDK .c)

    uint32_t reserved_0x24_0x7c[23];    // JPEG_REG9–REG31 not defined in SDK

    uint32_t quantization_table[32];    // quantization table RAM (0x0080A080–0x0080A0FC); written by ejpeg_init_quant_table(); survives WDT reset (confirmed on hardware — SDK uses quantization_table[0] as start_type scratch on BK7221U)

} hw_jpeg_t;

#define hw_jpeg  ((volatile hw_jpeg_t *) JPEG_BASE_ADDR)
