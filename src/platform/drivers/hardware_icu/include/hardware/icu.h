#ifndef _HARDWARE_ICU_H
#define _HARDWARE_ICU_H

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>

#define ICU_BASE                                    (0x00802000)
#define ICU_PERI_CLK_MUX                            (ICU_BASE + 0 * 4)
#define ICU_PWM_CLK_MUX                             (ICU_BASE + 1 * 4)
#define ICU_PERI_CLK_PWD                            (ICU_BASE + 2 * 4)
#define ICU_PERI_CLK_GATING                         (ICU_BASE + 3 * 4)
#define ICU_TL410_CLK_PWD                           (ICU_BASE + 4 * 4)
#define ICU_CLK26M_DIV_FACTOR                       (ICU_BASE + 5 * 4)
#define ICU_JTAG_SELECT                             (ICU_BASE + 6 * 4)
//#define ICU_INTERRUPT_ENABLE                        (ICU_BASE + 16 * 4)
//#define ICU_GLOBAL_INT_EN                           (ICU_BASE + 17 * 4)
//#define ICU_INT_RAW_STATUS                          (ICU_BASE + 18 * 4)
//#define ICU_INT_STATUS                              (ICU_BASE + 19 * 4)
//#define ICU_ARM_WAKEUP_EN                           (ICU_BASE + 20 * 4)
//#define ICU_TL4_INT_EN                              (ICU_BASE + 32 * 4)
//#define ICU_TL4_INT_RAW_STATUS                      (ICU_BASE + 33 * 4)
//#define ICU_TL4_INT_STATUS                          (ICU_BASE + 34 * 4)
//#define ICU_TL4_WAKEUP_EN                           (ICU_BASE + 35 * 4)


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

typedef struct {
    // peripheral clock select
    union {
        uint32_t reg;
        struct __packed {
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
        struct __packed {
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
        };
    } peri_clk_pwd;

    // ?
    union {
        uint32_t reg;
        struct __packed {
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
        struct __packed {
            uint32_t tl410: 1;
            uint32_t ble: 1;
        };
    } tl410_clk_pwd;
} hw_icu_t;

#define hw_icu          ((volatile hw_icu_t *)ICU_BASE)


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