#pragma once

#include "platform/soc.h"


#define ICU_BASE_ADDR (0x00802000)


typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t uart1 : 1;           // [0]
            uint32_t uart2 : 1;           // [1]
            uint32_t i2c1 : 1;            // [2]   1=CLK_26M, 0=DCO divided
            uint32_t irda : 1;            // [3]
            uint32_t i2c2 : 1;            // [4]
            uint32_t saradc : 1;          // [5]
            uint32_t spi : 1;             // [6]
            uint32_t pwms : 1;            // [7]
            uint32_t sdio : 1;            // [8]
            uint32_t saradc_aud : 1;      // [9]   Audio PLL select (valid when saradc=0)
            uint32_t reserved_10_15 : 6;  // [15:10]
            uint32_t qspi : 2;            // [17:16]  0=DCO, 1=26M, 2=120M (JPEG?)
            uint32_t dco_clk_div : 2;     // [19:18]  0=/1, 1=/2, 2=/4, 3=/8
            uint32_t reserved_20_31 : 12; // [31:20]
        };
    } peri_clk_mux;

    uint32_t pwm_clk_mux; // 0=PCLK, 1=LPO

    union {
        uint32_t v;
        struct {
            uint32_t uart1 : 1;          // [0]
            uint32_t uart2 : 1;          // [1]
            uint32_t i2c1 : 1;           // [2]
            uint32_t irda : 1;           // [3]
            uint32_t i2s_pcm : 1;        // [4]
            uint32_t i2c2 : 1;           // [5]
            uint32_t spi : 1;            // [6]
            uint32_t saradc : 1;         // [7]
            uint32_t arm_wdt : 1;        // [8]
            uint32_t pwm0 : 1;           // [9]
            uint32_t pwm1 : 1;           // [10]
            uint32_t pwm2 : 1;           // [11]
            uint32_t pwm3 : 1;           // [12]
            uint32_t pwm4 : 1;           // [13]
            uint32_t pwm5 : 1;           // [14]
            uint32_t audio : 1;          // [15]  Security?
            uint32_t tl410_wdt : 1;      // [16]
            uint32_t sdio : 1;           // [17]
            uint32_t usb : 1;            // [18]
            uint32_t fft : 1;            // [19]
            uint32_t timer_26m : 1;      // [20]
            uint32_t timer_32k : 1;      // [21]
            uint32_t jpeg_encoder : 1;   // [22]
            uint32_t qspi : 1;           // [23]
            uint32_t reserved_24_31 : 8; // [31:24]
        };
    } peri_clk_pwd;

    union {
        uint32_t v;
        struct {
            uint32_t icu : 1;             // [0]
            uint32_t uart1 : 1;           // [1]
            uint32_t uart2 : 1;           // [2]
            uint32_t i2c1 : 1;            // [3]
            uint32_t irda : 1;            // [4]
            uint32_t i2s_pcm : 1;         // [5]
            uint32_t i2c2 : 1;            // [6]
            uint32_t spi : 1;             // [7]
            uint32_t gpio : 1;            // [8]
            uint32_t wdt : 1;             // [9]
            uint32_t pwm : 1;             // [10]  timers?
            uint32_t audio : 1;           // [11]  PWMs?
            uint32_t saradc : 1;          // [12]
            uint32_t sdio : 1;            // [13]
            uint32_t usb : 1;             // [14]
            uint32_t fft : 1;             // [15]
            uint32_t mac : 1;             // [16]
            uint32_t reserved_17_31 : 15; // [31:17]
        };
    } peri_clk_gate_disable;

    union {
        uint32_t v;
        struct {
            uint32_t tl410 : 1;          // [0]
            uint32_t ble : 1;            // [1]
            uint32_t reserved_2_31 : 30; // [31:2]
        };
    } tl410_clk_pwd;

    uint32_t clk26m_divider; // 0=/1, 1=/2, 2=/4, 3=/8
    uint32_t jtag_select;    // write magic value; read 0=ARM, 1=TL4
} hw_icu_t;

#define hw_icu ((volatile hw_icu_t *)ICU_BASE_ADDR)
