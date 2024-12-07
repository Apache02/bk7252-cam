#include "hardware/intc.h"
#include "utils/panic.h"
#include <stdio.h>
#include <string.h>

#include "intc_regs.h"
#include "intc_manager.h"


extern void do_irq(void);
extern void do_fiq(void);
extern void do_swi(void);
extern void do_undefined(void);
extern void do_pabort(void);
extern void do_dabort(void);
extern void do_reserved(void);


void intc_init() {
    memset(&intc_manager, 0, sizeof(intc_manager));

    bk_interrupt_trap_ram->irq = do_irq;
    bk_interrupt_trap_ram->fiq = do_fiq;
    bk_interrupt_trap_ram->swi = do_swi;
    bk_interrupt_trap_ram->undefined = do_undefined;
    bk_interrupt_trap_ram->pabort = do_pabort;
    bk_interrupt_trap_ram->dabort = do_dabort;
    bk_interrupt_trap_ram->reserved = do_reserved;

    icu_interrupt_enable_reg->v = 0;

    icu_interrupt_enable_reg->fiq_mac_general = 1;
    icu_interrupt_enable_reg->fiq_mac_prot_trigger = 1;

    icu_interrupt_enable_reg->fiq_mac_tx_trigger = 1;
    icu_interrupt_enable_reg->fiq_mac_rx_trigger = 1;

    icu_interrupt_enable_reg->fiq_mac_tx_rx_misc = 1;
    icu_interrupt_enable_reg->fiq_mac_tx_rx_timer = 1;

    icu_interrupt_enable_reg->fiq_modem = 1;

    icu_global_interrupt_enable_reg->v = GINTR_IRQ_EN | GINTR_FIQ_EN;
}

void intc_irq(void) {
    icu_interrupts_reg_t status = *icu_interrupt_status_reg;

    status.v &= ICU_INT_IRQ_MASK;
    if (!status.v) {
        panic("irq:dead\r\n");
    }

    // clear status
    icu_interrupt_status_reg->v |= status.v;

    uint32_t source = 0;
    if (status.irq_uart1) source |= IRQ_SOURCE_UART1;
    if (status.irq_uart2) source |= IRQ_SOURCE_UART2;
    if (status.irq_i2c1) source |= IRQ_SOURCE_I2C1;
    if (status.irq_irda) source |= IRQ_SOURCE_IRDA;
    if (status.irq_i2s_pcm) source |= IRQ_SOURCE_I2S_PCM;
    if (status.irq_i2c2) source |= IRQ_SOURCE_I2C2;
    if (status.irq_spi) source |= IRQ_SOURCE_SPI;
    if (status.irq_gpio) source |= IRQ_SOURCE_GPIO;
    if (status.irq_timer) source |= IRQ_SOURCE_TIMER;
    if (status.irq_pwm) source |= IRQ_SOURCE_PWM;
    if (status.irq_audio) source |= IRQ_SOURCE_AUDIO;
    if (status.irq_saradc) source |= IRQ_SOURCE_SARADC;
    if (status.irq_sdio) source |= IRQ_SOURCE_SDIO;
    if (status.irq_usb) source |= IRQ_SOURCE_USB;
    if (status.irq_fft) source |= IRQ_SOURCE_FFT;
    if (status.irq_gdma) source |= IRQ_SOURCE_GDMA;

    if (!source) return;

    interrupt_handler_cb *handlers[MAX_HANDLERS] = {0};
    int count = 0;

    for (int i = 0; i < intc_manager.irq.count; i++) {
        if ((intc_manager.irq.handlers[i].source & source) != 0) {
            handlers[i] = intc_manager.irq.handlers[i].handler;
            count++;
        }
    }

    interrupt_context_t context = {
            .type = INTERRUPT_TYPE_IRQ,
            .source = source,
    };

    for (int i = 0; i < count; i++) {
        handlers[i](context);
    }
}

void intc_fiq(void) {
    panic(__FUNCTION__);
}

bool intc_register_irq_handler(uint32_t source, interrupt_handler_cb *func) {
    if (intc_manager.irq.count >= MAX_HANDLERS) return false;
    return register_handler(&intc_manager.irq, source, func);
}

bool intc_register_fiq_handler(uint32_t source, interrupt_handler_cb *func) {
    if (intc_manager.fiq.count >= MAX_HANDLERS) return false;
    return register_handler(&intc_manager.fiq, source, func);
}

bool intc_unregister_irq_handler(uint32_t source, interrupt_handler_cb *func) {
    return unregister_handler(&intc_manager.irq, source, func);
}

bool intc_unregister_fiq_handler(uint32_t source, interrupt_handler_cb *func) {
    return unregister_handler(&intc_manager.fiq, source, func);
}

static uint32_t source_to_reg(uint32_t source) {
    volatile icu_interrupts_reg_t reg = {0};
    if (source & IRQ_SOURCE_UART1) reg.irq_uart1 = 1;
    if (source & IRQ_SOURCE_UART2) reg.irq_uart2 = 1;
    if (source & IRQ_SOURCE_I2C1) reg.irq_i2c1 = 1;
    if (source & IRQ_SOURCE_IRDA) reg.irq_irda = 1;
    if (source & IRQ_SOURCE_I2S_PCM) reg.irq_i2s_pcm = 1;
    if (source & IRQ_SOURCE_I2C2) reg.irq_i2c2 = 1;
    if (source & IRQ_SOURCE_SPI) reg.irq_spi = 1;
    if (source & IRQ_SOURCE_GPIO) reg.irq_gpio = 1;
    if (source & IRQ_SOURCE_TIMER) reg.irq_timer = 1;
    if (source & IRQ_SOURCE_PWM) reg.irq_pwm = 1;
    if (source & IRQ_SOURCE_AUDIO) reg.irq_audio = 1;
    if (source & IRQ_SOURCE_SARADC) reg.irq_saradc = 1;
    if (source & IRQ_SOURCE_SDIO) reg.irq_sdio = 1;
    if (source & IRQ_SOURCE_USB) reg.irq_usb = 1;
    if (source & IRQ_SOURCE_FFT) reg.irq_fft = 1;
    if (source & IRQ_SOURCE_GDMA) reg.irq_gdma = 1;

    return reg.v;
}

void intc_enable_irq_source(uint32_t source) {
    icu_interrupt_enable_reg->v |= source_to_reg(source);
}

void intc_disable_irq_source(uint32_t source) {
    icu_interrupt_enable_reg->v &= ~source_to_reg(source);
}
