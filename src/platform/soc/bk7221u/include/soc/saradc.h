#pragma once

#include "platform/soc.h"


#define SARADC_BASE_ADDR (0x00802C00)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t mode: 2;        // [1:0]
            uint32_t chnl_en: 1;     // [2]     1 = channel conversion enabled; clearing it stops the channel
            uint32_t chnl: 4;        // [6:3]   channel number, 0..12
            uint32_t setting: 1;     // [7]
            uint32_t int_clr: 1;     // [8]     write 1 to clear the interrupt; reads back 0 once cleared
            uint32_t pre_div: 6;     // [14:9]
            uint32_t mode_32m: 1;    // [15]    SDK name SARADC_ADC_32M_MODE
            uint32_t sample_rate: 6; // [21:16]
            uint32_t filter: 7;      // [28:22]
            uint32_t busy: 1;        // [29]
            uint32_t fifo_empty: 1;  // [30]    1 = no sample in the FIFO
            uint32_t fifo_full: 1;   // [31]
        };
    } config;

    uint32_t data; // sample FIFO output; the driver reads it until config.fifo_empty is set

    union {
        uint32_t v;
        struct {
            uint32_t almost_cfg: 5;     // [4:0]
            uint32_t steady_ctrl: 3;    // [7:5]
            uint32_t reserved_8_31: 24; // [31:8]
        };
    } ctrl_cfg;

    union {
        uint32_t v;
        struct {
            uint32_t sat_ctrl: 2;       // [1:0]
            uint32_t sat_enable: 1;     // [2]
            uint32_t sat_overflow: 1;   // [3]
            uint32_t reserved_4_31: 28; // [31:4]
        };
    } saturation_cfg;

    uint32_t dat_after_sta; // read together with data while draining the FIFO
} hw_saradc_t;

static_assert(sizeof(hw_saradc_t) == 5 * sizeof(uint32_t), "hw_saradc_t size mismatch");

#define hw_saradc ((volatile hw_saradc_t *)SARADC_BASE_ADDR)
