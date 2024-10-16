#ifndef _HARDWARE_ICU_H
#define _HARDWARE_ICU_H

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>

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

typedef struct {
    // peripheral clock select
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            peri_clk_t uart1: 1;
            peri_clk_t uart2: 1;
            peri_clk_t i2c1: 1;
            peri_clk_t irda: 1;
            peri_clk_t i2c2: 1;
            peri_clk_t saradc: 1;
            peri_clk_t spi: 1;
            peri_clk_t pwms: 1;
            peri_clk_t sdio: 1;
            peri_clk_t saradc_aud: 1;
            uint32_t _pad0: (16 - 10);
            qspi_clk_t qspi: 2;
            dco_divider_t dco_divider: 2;
            //
        } bits;
    } peri_clk_mux;

    // PWMs clock
    union {
        uint32_t reg;
        pwm_clk_t value;
    } pwm_clk_mux;

    // clocks power down
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t uart1: 1;
            uint32_t uart2: 1;
            uint32_t i2c1: 1;
            uint32_t irda: 1;
            uint32_t i2s_pcm: 1;
            uint32_t i2c2: 1;
            uint32_t spi: 1;
            uint32_t saradc: 1;
            uint32_t arm_wdt: 1;
            uint32_t pwm0: 1;
            uint32_t pwm1: 1;
            uint32_t pwm2: 1;
            uint32_t pwm3: 1;
            uint32_t pwm4: 1;
            uint32_t pwm5: 1;
            uint32_t audio: 1;
            uint32_t tl410_wdt: 1;
            uint32_t sdio: 1;
            uint32_t usb: 1;
            uint32_t fft: 1;
            uint32_t timer_26m: 1;
            uint32_t timer_32k: 1;
            uint32_t _unk0: 1;
            uint32_t _unk1: 1;
            uint32_t reserved_24_31: 8;
        } bits;
    } peri_clk_pwd;

    // ?
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t disable_icu_apb: 1;
            uint32_t disable_uart1_apb: 1;
            uint32_t disable_uart2_apb: 1;
            uint32_t disable_i2c1_apb: 1;
            uint32_t disable_irda_apb: 1;
            uint32_t disable_i2s_pcm_apb: 1;
            uint32_t disable_i2c2_apb: 1;
            uint32_t disable_spi_apb: 1;
            uint32_t disable_gpio_apb: 1;
            uint32_t disable_wdt_apb: 1;
            uint32_t disable_pwm_apb: 1;
            uint32_t disable_audio_apb: 1;
            uint32_t disable_saradc_apb: 1;
            uint32_t disable_sdio_ahb: 1;
            uint32_t disable_usb_ahb: 1;
            uint32_t disable_fft_ahb: 1;
            uint32_t disable_mac_ahb: 1;
        };
    } peri_clk_gating;

    // clocks power down 2
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t tl410: 1;
            uint32_t ble: 1;
        };
    } tl410_clk_pwd;

    union {
        uint32_t reg;
        clk26m_div_t value;
    } clk26m_divider;

    union {
        uint32_t reg;
        jtag_select_t value;
    } jtag_select;

} hw_icu_t;

#define hw_icu          ((volatile hw_icu_t *)ICU_BASE)

/*******************************************************************/

#ifdef __cplusplus
extern "C" {
#endif


inline void icu_peri_clk_uart1(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.uart1 = clk;
}

inline void icu_peri_clk_uart2(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.uart2 = clk;
}

inline void icu_peri_clk_i2c1(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.i2c1 = clk;
}

inline void icu_peri_clk_i2c2(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.i2c2 = clk;
}

inline void icu_peri_clk_irda(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.irda = clk;
}

inline void icu_peri_clk_saradc(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.saradc = clk;
}

inline void icu_peri_clk_saradc_aud(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.saradc_aud = clk;
}

inline void icu_peri_clk_spi(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.spi = clk;
}

inline void icu_peri_clk_pwms(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.pwms = clk;
}

inline void icu_peri_clk_sdio(peri_clk_t clk) {
    hw_icu->peri_clk_mux.bits.sdio = clk;
}

inline void icu_qspi_clk(qspi_clk_t clk) {
    hw_icu->peri_clk_mux.bits.qspi = clk;
}

inline void icu_dco_divider(dco_divider_t divider) {
    hw_icu->peri_clk_mux.bits.dco_divider = divider;
}


#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_ICU_H