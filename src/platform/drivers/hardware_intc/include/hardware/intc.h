#ifndef _HARDWARE_INTC_H
#define _HARDWARE_INTC_H

#include <stdbool.h>
#include <stdint.h>


enum {
    INTERRUPT_TYPE_IRQ,
    INTERRUPT_TYPE_FIQ,
};

enum {
    IRQ_SOURCE_NONE = 0,
    IRQ_SOURCE_UART1 = (1 << 0),
    IRQ_SOURCE_UART2 = (1 << 1),
    IRQ_SOURCE_I2C1 = (1 << 2),
    IRQ_SOURCE_IRDA = (1 << 3),
    IRQ_SOURCE_I2S_PCM = (1 << 4),
    IRQ_SOURCE_I2C2 = (1 << 5),
    IRQ_SOURCE_SPI = (1 << 6),
    IRQ_SOURCE_GPIO = (1 << 7),
    IRQ_SOURCE_TIMER = (1 << 8),
    IRQ_SOURCE_PWM = (1 << 9),
    IRQ_SOURCE_AUDIO = (1 << 10),
    IRQ_SOURCE_SARADC = (1 << 11),
    IRQ_SOURCE_SDIO = (1 << 12),
    IRQ_SOURCE_USB = (1 << 13),
    IRQ_SOURCE_FFT = (1 << 14),
    IRQ_SOURCE_GDMA = (1 << 15),
};

enum {
    FIQ_SOURCE_NONE = 0,
    FIQ_SOURCE_MODEM = (1 << 16),
    FIQ_SOURCE_MAC_TX_RX_TIMER = (1 << 17),
    FIQ_SOURCE_MAC_TX_RX_MISC = (1 << 18),
    FIQ_SOURCE_MAC_RX_TRIGGER = (1 << 19),
    FIQ_SOURCE_MAC_TX_TRIGGER = (1 << 20),
    FIQ_SOURCE_MAC_PROT_TRIGGER = (1 << 21),
    FIQ_SOURCE_MAC_GENERAL = (1 << 22),
    FIQ_SOURCE_SDIO_DMA = (1 << 23),
    FIQ_SOURCE_USB_PLUG_INOUT = (1 << 24),
    FIQ_SOURCE_SECURITY = (1 << 25),
    FIQ_SOURCE_MAC_WAKE_UP = (1 << 26),
    FIQ_SOURCE_SPI_DMA = (1 << 27),
    FIQ_SOURCE_DPLL_UNLOCK = (1 << 28),
};

typedef struct {
    uint32_t source;
    uint8_t type;
} interrupt_context_t;

typedef void interrupt_handler_cb(interrupt_context_t context);


#ifdef __cplusplus
extern "C" {
#endif

void intc_init();

bool intc_register_irq_handler(uint32_t source, interrupt_handler_cb *func);

bool intc_register_fiq_handler(uint32_t source, interrupt_handler_cb *func);

bool intc_unregister_irq_handler(uint32_t source, interrupt_handler_cb *func);

bool intc_unregister_fiq_handler(uint32_t source, interrupt_handler_cb *func);

void intc_enable_irq_source(uint32_t source);

void intc_disable_irq_source(uint32_t source);

void intc_enable_fiq_source(uint32_t source);

void intc_disable_fiq_source(uint32_t source);

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_INTC_H
