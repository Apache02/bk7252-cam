#pragma once

#include "platform/soc.h"
#include "soc/icu.h"


#define ICU_INTERRUPT_ENABLE (ICU_BASE_ADDR + 16 * 4)
#define ICU_INT_IRQ_MASK     (0x0000FFFF)
#define ICU_INT_FIQ_MASK     (0xFFFF0000)

#define ICU_GLOBAL_INT_EN (ICU_BASE_ADDR + 17 * 4)
#define GINTR_FIQ_EN      (1 << 1)
#define GINTR_IRQ_EN      (1 << 0)

#define ICU_INT_RAW_STATUS (ICU_BASE_ADDR + 18 * 4)
#define ICU_INT_STATUS     (ICU_BASE_ADDR + 19 * 4)


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
} hw_intc_irq_mask_t;

typedef volatile struct {
    union {
        uint32_t v;
        struct {
            uint32_t irq : 1;            // [0]
            uint32_t fiq : 1;            // [1]
            uint32_t reserved_2_31 : 30; // [31:2]
        };
    };
} hw_intc_global_en_t;

#define hw_intc_global_en  ((volatile hw_intc_global_en_t *)ICU_GLOBAL_INT_EN)
#define hw_intc_enable     ((volatile hw_intc_irq_mask_t *)ICU_INTERRUPT_ENABLE)
#define hw_intc_raw_status ((volatile hw_intc_irq_mask_t *)ICU_INT_RAW_STATUS)
#define hw_intc_status     ((volatile hw_intc_irq_mask_t *)ICU_INT_STATUS)
