#ifndef _HARDWARE_ICU_H
#define _HARDWARE_ICU_H

#include "register_defs.h"


/*******************************************************************/

#define ICU_BASE                    (0x00802000)

/*******************************************************************/

typedef enum {
    PERI_CLK_DCO = 0,
    PERI_CLK_26M_XTAL = 1
} peri_clk_t;

typedef enum {
    QSPI_CLK_MUX_DCO = 0,
    QSPI_CLK_MUX_26M = 1,
    QSPI_CLK_MUX_120M = 2,
} qspi_clk_t;

typedef enum {
    DCO_DIVIDER_1 = 0,
    DCO_DIVIDER_2,
    DCO_DIVIDER_4,
    DCO_DIVIDER_8,
} dco_divider_t;

typedef enum {
    PWM_CLK_PCLK = 0,
    PWM_CLK_LPO = 1,
} pwm_clk_t;

typedef enum {
    CLK26M_DIVIDER_1 = 0,
    CLK26M_DIVIDER_2,
    CLK26M_DIVIDER_4,
    CLK26M_DIVIDER_8,
} clk26m_div_t;

typedef enum {
    JTAG_SEL_WR_ARM = 0x7111E968,
    JTAG_SEL_WR_TL4 = 0x7111E410,
    JTAG_SEL_RD_ARM = 0x00000000,
    JTAG_SEL_RD_TL4 = 0x00000001,
} jtag_select_t;

/*******************************************************************/

typedef volatile struct {
    // peripheral clock select
    union {
        uint32_t v;
        struct _register_bits {
            peri_clk_t uart1: 1;                // [0]
            peri_clk_t uart2: 1;                // [1]
            peri_clk_t i2c1: 1;                 // [2]
            peri_clk_t irda: 1;                 // [3]
            peri_clk_t i2c2: 1;                 // [4]
            peri_clk_t saradc: 1;               // [5]
            peri_clk_t spi: 1;                  // [6]
            peri_clk_t pwms: 1;                 // [7]
            peri_clk_t sdio: 1;                 // [8]
            peri_clk_t saradc_aud: 1;           // [9]
            uint32_t reserved_10_15: 6;         // [10:15]
            qspi_clk_t qspi: 2;                 // [16:17]
            dco_divider_t dco_divider: 2;       // [18:19]
            uint32_t reserved_20_31: 12;        // [20:31]
        };
    } peri_clk_mux;

    // PWMs clock
    union {
        uint32_t v;
        pwm_clk_t value;
    } pwm_clk_mux;

    // clocks power down
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t uart1: 1;                  // [0]
            uint32_t uart2: 1;                  // [1]
            uint32_t i2c1: 1;                   // [2]
            uint32_t irda: 1;                   // [3]
            uint32_t i2s_pcm: 1;                // [4]
            uint32_t i2c2: 1;                   // [5]
            uint32_t spi: 1;                    // [6]
            uint32_t saradc: 1;                 // [7]
            uint32_t arm_wdt: 1;                // [8]
            uint32_t pwm0: 1;                   // [9]
            uint32_t pwm1: 1;                   // [10]
            uint32_t pwm2: 1;                   // [11]
            uint32_t pwm3: 1;                   // [12]
            uint32_t pwm4: 1;                   // [13]
            uint32_t pwm5: 1;                   // [14]
            uint32_t audio: 1;                  // [15]
            uint32_t tl410_wdt: 1;              // [16]
            uint32_t sdio: 1;                   // [17]
            uint32_t usb: 1;                    // [18]
            uint32_t fft: 1;                    // [19]
            uint32_t timer_26m: 1;              // [20]
            uint32_t timer_32k: 1;              // [21]
            uint32_t unknown_22: 1;             // [22]
            uint32_t unknown_23: 1;             // [23]
            uint32_t reserved_24_31: 8;         // [24:31]
        };
    } peri_clk_pwd;

    // ?
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t disable_icu_apb: 1;        // [0]
            uint32_t disable_uart1_apb: 1;      // [1]
            uint32_t disable_uart2_apb: 1;      // [2]
            uint32_t disable_i2c1_apb: 1;       // [3]
            uint32_t disable_irda_apb: 1;       // [4]
            uint32_t disable_i2s_pcm_apb: 1;    // [5]
            uint32_t disable_i2c2_apb: 1;       // [6]
            uint32_t disable_spi_apb: 1;        // [7]
            uint32_t disable_gpio_apb: 1;       // [8]
            uint32_t disable_wdt_apb: 1;        // [9]
            uint32_t disable_pwm_apb: 1;        // [10]
            uint32_t disable_audio_apb: 1;      // [11]
            uint32_t disable_saradc_apb: 1;     // [12]
            uint32_t disable_sdio_ahb: 1;       // [13]
            uint32_t disable_usb_ahb: 1;        // [14]
            uint32_t disable_fft_ahb: 1;        // [15]
            uint32_t disable_mac_ahb: 1;        // [16]
            uint32_t reserved_17_31: 15;        // [17:31]
        };
    } peri_clk_gating;

    // clocks power down 2
    union {
        uint32_t v;
        struct _register_bits {
            uint32_t tl410: 1;                  // [0]
            uint32_t ble: 1;                    // [1]
            uint32_t reserved_2_31: 30;         // [2:31]
        };
    } tl410_clk_pwd;

    union {
        uint32_t v;
        clk26m_div_t value;
    } clk26m_divider;

    union {
        uint32_t v;
        jtag_select_t value;
    } jtag_select;

} hw_icu_t;

#define hw_icu          ((volatile hw_icu_t *)ICU_BASE)

/*******************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


inline void icu_peri_clk_uart1(peri_clk_t clk) {
    hw_icu->peri_clk_mux.uart1 = clk;
}

inline void icu_peri_clk_uart2(peri_clk_t clk) {
    hw_icu->peri_clk_mux.uart2 = clk;
}

inline void icu_i2c1_set_clk_source(peri_clk_t clk) {
    hw_icu->peri_clk_mux.i2c1 = clk;
}

inline void icu_peri_clk_i2c2(peri_clk_t clk) {
    hw_icu->peri_clk_mux.i2c2 = clk;
}

inline void icu_peri_clk_irda(peri_clk_t clk) {
    hw_icu->peri_clk_mux.irda = clk;
}

inline void icu_peri_clk_saradc(peri_clk_t clk) {
    hw_icu->peri_clk_mux.saradc = clk;
}

inline void icu_peri_clk_saradc_aud(peri_clk_t clk) {
    hw_icu->peri_clk_mux.saradc_aud = clk;
}

inline void icu_peri_clk_spi(peri_clk_t clk) {
    hw_icu->peri_clk_mux.spi = clk;
}

inline void icu_peri_clk_pwms(peri_clk_t clk) {
    hw_icu->peri_clk_mux.pwms = clk;
}

inline void icu_peri_clk_sdio(peri_clk_t clk) {
    hw_icu->peri_clk_mux.sdio = clk;
}

inline void icu_qspi_clk(qspi_clk_t clk) {
    hw_icu->peri_clk_mux.qspi = clk;
}

inline void icu_dco_divider(dco_divider_t divider) {
    hw_icu->peri_clk_mux.dco_divider = divider;
}

inline void icu_i2c1_power_up() {
    hw_icu->peri_clk_pwd.i2c1 = 0;
}

inline void icu_i2c1_power_down() {
    hw_icu->peri_clk_pwd.i2c1 = 1;
}


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_ICU_H