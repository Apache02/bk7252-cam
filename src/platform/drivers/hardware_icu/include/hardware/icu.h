#pragma once

#include "soc/icu.h"


typedef enum { PERI_CLK_DCO = 0, PERI_CLK_26M_XTAL = 1 } peri_clk_t;

typedef enum {
    QSPI_CLK_MUX_DCO  = 0,
    QSPI_CLK_MUX_26M  = 1,
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
    PWM_CLK_LPO  = 1,
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


#ifdef __cplusplus
extern "C" {
#endif

static inline void icu_uart1_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.uart1 = clk; }

static inline void icu_uart2_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.uart2 = clk; }

static inline void icu_i2c1_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.i2c1 = clk; }

static inline void icu_i2c2_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.i2c2 = clk; }

static inline void icu_irda_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.irda = clk; }

static inline void icu_saradc_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.saradc = clk; }

static inline void icu_saradc_aud_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.saradc_aud = clk; }

static inline void icu_spi_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.spi = clk; }

static inline void icu_pwms_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.pwms = clk; }

static inline void icu_sdio_clk(peri_clk_t clk) { hw_icu->peri_clk_mux.sdio = clk; }

static inline void icu_qspi_clk(qspi_clk_t clk) { hw_icu->peri_clk_mux.qspi = clk; }

static inline void icu_dco_divider(dco_divider_t divider) { hw_icu->peri_clk_mux.dco_clk_div = divider; }

static inline void icu_uart1_power_up() { hw_icu->peri_clk_pwd.uart1 = 0; }

static inline void icu_uart1_power_down() { hw_icu->peri_clk_pwd.uart1 = 1; }

static inline void icu_uart2_power_up() { hw_icu->peri_clk_pwd.uart2 = 0; }

static inline void icu_uart2_power_down() { hw_icu->peri_clk_pwd.uart2 = 1; }

static inline void icu_i2c1_power_up() { hw_icu->peri_clk_pwd.i2c1 = 0; }

static inline void icu_i2c1_power_down() { hw_icu->peri_clk_pwd.i2c1 = 1; }

#ifdef __cplusplus
}
#endif
