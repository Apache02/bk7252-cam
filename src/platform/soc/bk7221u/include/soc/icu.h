#pragma once

#include "platform/soc.h"


#define ICU_BASE_ADDR (0x00802000)

// IRQ sources occupy bits [15:0] of the interrupt registers; FIQ sources [31:16]
#define ICU_INT_IRQ_MASK  (0x0000FFFF)
#define ICU_INT_FIQ_MASK  (0xFFFF0000)


// Shared layout for irq_enable, irq_raw_status, and irq_status (all three have identical bit positions)
typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t irq_uart1 : 1;            // [0]
            uint32_t irq_uart2 : 1;            // [1]
            uint32_t irq_i2c1 : 1;             // [2]
            uint32_t irq_irda : 1;             // [3]
            uint32_t irq_i2s_pcm : 1;          // [4]
            uint32_t irq_i2c2 : 1;             // [5]
            uint32_t irq_spi : 1;              // [6]
            uint32_t irq_gpio : 1;             // [7]
            uint32_t irq_timer : 1;            // [8]
            uint32_t irq_pwm : 1;              // [9]
            uint32_t irq_audio : 1;            // [10]
            uint32_t irq_saradc : 1;           // [11]
            uint32_t irq_sdio : 1;             // [12]
            uint32_t irq_usb : 1;              // [13]
            uint32_t irq_fft : 1;              // [14]
            uint32_t irq_gdma : 1;             // [15]
            uint32_t fiq_modem : 1;            // [16]
            uint32_t fiq_mac_tx_rx_timer : 1;  // [17]
            uint32_t fiq_mac_tx_rx_misc : 1;   // [18]
            uint32_t fiq_mac_rx_trigger : 1;   // [19]
            uint32_t fiq_mac_tx_trigger : 1;   // [20]
            uint32_t fiq_mac_prot_trigger : 1; // [21]
            uint32_t fiq_mac_general : 1;      // [22]
            uint32_t fiq_sdio_dma : 1;         // [23]
            uint32_t fiq_usb_plug_inout : 1;   // [24]
            uint32_t fiq_security : 1;         // [25]
            uint32_t fiq_mac_wake_up : 1;      // [26]
            uint32_t fiq_spi_dma : 1;          // [27]
            uint32_t fiq_dpll_unlock : 1;      // [28]
            uint32_t jpeg_encoder : 1;         // [29]
            uint32_t ble : 1;                  // [30]
            uint32_t psram : 1;                // [31]
        };
    };
} hw_icu_int_t;


typedef volatile struct {
    // --- clock mux / power-down / gating (offsets 0–6) ---

    union {
        uint32_t v;
        struct {
            uint32_t uart1 : 1;           // [0]   1=26 MHz XTAL, 0=DCO
            uint32_t uart2 : 1;           // [1]
            uint32_t i2c1 : 1;            // [2]
            uint32_t irda : 1;            // [3]
            uint32_t i2c2 : 1;            // [4]
            uint32_t saradc : 1;          // [5]
            uint32_t spi : 1;             // [6]
            uint32_t pwms : 1;            // [7]
            uint32_t sdio : 1;            // [8]
            uint32_t saradc_aud : 1;      // [9]   audio PLL select (valid when saradc=0)
            uint32_t reserved_10_15 : 6;  // [15:10]
            uint32_t qspi : 2;            // [17:16]  0=DCO, 1=26 MHz, 2=120 MHz
            uint32_t dco_clk_div : 2;     // [19:18]  0=/1, 1=/2, 2=/4, 3=/8
            uint32_t reserved_20_31 : 12; // [31:20]
        };
    } peri_clk_mux;

    union {
        uint32_t v;
        struct {
            uint32_t pwm0 : 1;            // [0]   0=PCLK, 1=LPO (32 kHz)
            uint32_t pwm1 : 1;            // [1]
            uint32_t pwm2 : 1;            // [2]
            uint32_t pwm3 : 1;            // [3]
            uint32_t pwm4 : 1;            // [4]
            uint32_t pwm5 : 1;            // [5]   one bit per channel
            uint32_t reserved_6_31 : 26;  // [31:6]
        };
    } pwm_clk_mux;

    union {
        uint32_t v;
        struct {
            uint32_t uart1 : 1;          // [0]   1=powered down
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
            uint32_t audio : 1;          // [15]
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
            uint32_t icu : 1;             // [0]   1=gate disabled (clock always on)
            uint32_t uart1 : 1;           // [1]
            uint32_t uart2 : 1;           // [2]
            uint32_t i2c1 : 1;            // [3]
            uint32_t irda : 1;            // [4]
            uint32_t i2s_pcm : 1;         // [5]
            uint32_t i2c2 : 1;            // [6]
            uint32_t spi : 1;             // [7]
            uint32_t gpio : 1;            // [8]
            uint32_t wdt : 1;             // [9]
            uint32_t pwm : 1;             // [10]
            uint32_t audio : 1;           // [11]
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
            uint32_t tl410 : 1;          // [0]   1=powered down
            uint32_t ble : 1;            // [1]
            uint32_t reserved_2_31 : 30; // [31:2]
        };
    } tl410_clk_pwd;

    union {
        uint32_t v;
        struct {
            uint32_t div : 2;             // [1:0]  0=/1, 1=/2, 2=/4, 3=/8
            uint32_t reserved_2_31 : 30;  // [31:2]
        };
    } clk26m_div;

    uint32_t jtag_select;               // write 0x7111E968=ARM, 0x7111E410=TL4; read 0=ARM, 1=TL4

    uint32_t reserved_0x1c_0x3c[9];    // offsets 0x1C–0x3C (words 7–15)

    // --- interrupt control (offsets 0x40–0x54) ---

    hw_icu_int_t irq_enable;            // which sources are forwarded to the ARM core
    union {
        uint32_t v;
        struct {
            uint32_t irq : 1;            // [0]   1=IRQ line enabled
            uint32_t fiq : 1;            // [1]   1=FIQ line enabled
            uint32_t reserved_2_31 : 30; // [31:2]
        };
    } global_int_en;
    hw_icu_int_t irq_raw_status;        // raw sources (before irq_enable mask); write 1 to clear
    hw_icu_int_t irq_status;            // masked sources (irq_raw_status & irq_enable); write 1 to clear

    // ARM deep-sleep wakeup enables — layout differs from irq_enable at bits [8,24,25,26]
    union {
        uint32_t v;
        struct {
            uint32_t uart1 : 1;            // [0]
            uint32_t uart2 : 1;            // [1]
            uint32_t i2c1 : 1;             // [2]
            uint32_t irda : 1;             // [3]
            uint32_t i2s_pcm : 1;          // [4]
            uint32_t i2c2 : 1;             // [5]
            uint32_t spi : 1;              // [6]
            uint32_t gpio : 1;             // [7]
            uint32_t tl410_wdt : 1;        // [8]
            uint32_t pwm : 1;              // [9]
            uint32_t audio : 1;            // [10]
            uint32_t saradc : 1;           // [11]
            uint32_t sdio : 1;             // [12]
            uint32_t usb : 1;              // [13]
            uint32_t fft : 1;              // [14]
            uint32_t gdma : 1;             // [15]
            uint32_t modem : 1;            // [16]
            uint32_t mac_tx_rx_timer : 1;  // [17]
            uint32_t mac_tx_rx_misc : 1;   // [18]
            uint32_t mac_rx_trigger : 1;   // [19]
            uint32_t mac_tx_trigger : 1;   // [20]
            uint32_t mac_prot_trigger : 1; // [21]
            uint32_t mac_general : 1;      // [22]
            uint32_t sdio_dma : 1;         // [23]
            uint32_t mailbox0 : 1;         // [24]
            uint32_t mailbox1 : 1;         // [25]
            uint32_t mac : 1;              // [26]
            uint32_t reserved_27_31 : 5;   // [31:27]
        };
    } arm_wakeup_en;
} hw_icu_t;

#define hw_icu ((volatile hw_icu_t *)ICU_BASE_ADDR)
