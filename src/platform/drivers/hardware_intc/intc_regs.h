#pragma once

#define ICU_BASE                            (0x00802000)
#define ICU_PERI_CLK_PWD                  (ICU_BASE + 2 * 4)
#define ICU_INTERRUPT_ENABLE                (ICU_BASE + 16 * 4)
#define ICU_INT_IRQ_MASK                    (0x0000FFFF);
#define ICU_INT_FIQ_MASK                    (0xFFFF0000);

#define ICU_GLOBAL_INT_EN                   (ICU_BASE + 17 * 4)
#define GINTR_FIQ_EN                        (1 << 1)
#define GINTR_IRQ_EN                        (1 << 0)

#define ICU_INT_RAW_STATUS                  (ICU_BASE + 18 * 4)
#define ICU_INT_STATUS                      (ICU_BASE + 19 * 4)


typedef void (intc_trap_cb)(void);

typedef struct {
    intc_trap_cb *irq;
    intc_trap_cb *fiq;
    intc_trap_cb *swi;
    intc_trap_cb *undefined;
    intc_trap_cb *pabort;
    intc_trap_cb *dabort;
    intc_trap_cb *reserved;
} bk_interrupt_trap_tbl_t;

#define bk_interrupt_trap_ram  ((volatile bk_interrupt_trap_tbl_t *)0x00400000)


typedef struct {
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t irq_uart1: 1;
            uint32_t irq_uart2: 1;
            uint32_t irq_i2c1: 1;
            uint32_t irq_irda: 1;
            uint32_t irq_i2s_pcm: 1;
            uint32_t irq_i2c2: 1;
            uint32_t irq_spi: 1;
            uint32_t irq_gpio: 1;
            uint32_t irq_timer: 1;
            uint32_t irq_pwm: 1;
            uint32_t irq_audio: 1;
            uint32_t irq_saradc: 1;
            uint32_t irq_sdio: 1;
            uint32_t irq_usb: 1;
            uint32_t irq_fft: 1;
            uint32_t irq_gdma: 1;
            uint32_t fiq_modem: 1;
            uint32_t fiq_mac_tx_rx_timer: 1;
            uint32_t fiq_mac_tx_rx_misc: 1;
            uint32_t fiq_mac_rx_trigger: 1;
            uint32_t fiq_mac_tx_trigger: 1;
            uint32_t fiq_mac_prot_trigger: 1;
            uint32_t fiq_mac_general: 1;
            uint32_t fiq_sdio_dma: 1;
            uint32_t fiq_usb_plug_inout: 1;
            uint32_t fiq_security: 1;
            uint32_t fiq_mac_wake_up: 1;
            uint32_t fiq_spi_dma: 1;
            uint32_t fiq_dpll_unlock: 1;
        } bits;
    };
} icu_interrupts_reg_t;

typedef struct {
    union {
        uint32_t reg;
        struct __attribute__((aligned(4))) __packed {
            uint32_t irq: 1;
            uint32_t fiq: 1;
        } bits;
    };
} icu_global_interrupt_enable_reg_t;

#define icu_global_interrupt_enable_reg     ((volatile icu_global_interrupt_enable_reg_t *)ICU_GLOBAL_INT_EN)
#define icu_interrupt_enable_reg            ((volatile icu_interrupts_reg_t *)ICU_INTERRUPT_ENABLE)
#define icu_interrupt_raw_status_reg        ((volatile icu_interrupts_reg_t *)ICU_INT_RAW_STATUS)
#define icu_interrupt_status_reg            ((volatile icu_interrupts_reg_t *)ICU_INT_STATUS)

