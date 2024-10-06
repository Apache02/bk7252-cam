#include "hardware/intc.h"
#include "entry/entry.h"
#include "utils/panic.h"
#include <stdio.h>
#include <string.h>

#include "intc_regs.h"
#include "int_handlers.h"
#include "intc_manager.h"


void intc_init() {
    memset(&intc_manager, 0, sizeof(intc_manager));

    bk_interrupt_trap_ram->irq = do_irq; // from platform_entry
    bk_interrupt_trap_ram->fiq = do_fiq; // from platform_entry
    bk_interrupt_trap_ram->swi = _do_swi;
    bk_interrupt_trap_ram->undefined = _do_undefined;
    bk_interrupt_trap_ram->pabort = _do_pabort;
    bk_interrupt_trap_ram->dabort = _do_dabort;
    bk_interrupt_trap_ram->reserved = _do_reserved;

    icu_interrupt_enable_reg->reg = 0;

    icu_interrupt_enable_reg->bits.fiq_mac_general = 1;
    icu_interrupt_enable_reg->bits.fiq_mac_prot_trigger = 1;

    icu_interrupt_enable_reg->bits.fiq_mac_tx_trigger = 1;
    icu_interrupt_enable_reg->bits.fiq_mac_rx_trigger = 1;

    icu_interrupt_enable_reg->bits.fiq_mac_tx_rx_misc = 1;
    icu_interrupt_enable_reg->bits.fiq_mac_tx_rx_timer = 1;

    icu_interrupt_enable_reg->bits.fiq_modem = 1;

    icu_global_interrupt_enable_reg->reg = GINTR_IRQ_EN | GINTR_FIQ_EN;
}

// called from platform_entry
void intc_irq(void) {
    icu_interrupts_reg_t status = *icu_interrupt_status_reg;

    status.reg = status.reg & ICU_INT_IRQ_MASK;
    if (!status.reg) {
        panic("irq:dead\r\n");
    }

    // clear status
    icu_interrupt_status_reg->reg |= status.reg;

    uint32_t source = 0;
    if (status.bits.irq_uart1) source |= IRQ_SOURCE_UART1;
    if (status.bits.irq_uart2) source |= IRQ_SOURCE_UART2;
    if (status.bits.irq_i2c1) source |= IRQ_SOURCE_I2C1;
    if (status.bits.irq_irda) source |= IRQ_SOURCE_IRDA;
    if (status.bits.irq_i2s_pcm) source |= IRQ_SOURCE_I2S_PCM;
    if (status.bits.irq_i2c2) source |= IRQ_SOURCE_I2C2;
    if (status.bits.irq_spi) source |= IRQ_SOURCE_SPI;
    if (status.bits.irq_gpio) source |= IRQ_SOURCE_GPIO;
    if (status.bits.irq_tl410_watchdog) source |= IRQ_SOURCE_TL410_WATCHDOG;
    if (status.bits.irq_pwm) source |= IRQ_SOURCE_PWM;
    if (status.bits.irq_audio) source |= IRQ_SOURCE_AUDIO;
    if (status.bits.irq_saradc) source |= IRQ_SOURCE_SARADC;
    if (status.bits.irq_sdio) source |= IRQ_SOURCE_SDIO;
    if (status.bits.irq_usb) source |= IRQ_SOURCE_USB;
    if (status.bits.irq_fft) source |= IRQ_SOURCE_FFT;
    if (status.bits.irq_gdma) source |= IRQ_SOURCE_GDMA;

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

// called from platform_entry
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
