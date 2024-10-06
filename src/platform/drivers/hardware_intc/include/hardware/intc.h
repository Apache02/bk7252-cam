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
    IRQ_SOURCE_TL410_WATCHDOG = (1 << 8), // TODO: check IRQ_TIMER
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

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_INTC_H
