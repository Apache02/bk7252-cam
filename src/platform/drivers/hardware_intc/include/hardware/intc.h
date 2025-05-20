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

#define IRQ_SOURCE_ALL      ( 0\
    | IRQ_SOURCE_UART1 \
    | IRQ_SOURCE_UART2 \
    | IRQ_SOURCE_I2C1 \
    | IRQ_SOURCE_IRDA \
    | IRQ_SOURCE_I2S_PCM \
    | IRQ_SOURCE_I2C2 \
    | IRQ_SOURCE_SPI \
    | IRQ_SOURCE_GPIO \
    | IRQ_SOURCE_TIMER \
    | IRQ_SOURCE_PWM \
    | IRQ_SOURCE_AUDIO \
    | IRQ_SOURCE_SARADC \
    | IRQ_SOURCE_SDIO \
    )

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
    FIQ_SOURCE_JPEG_ENCODER = (1 << 29),
    FIQ_SOURCE_BLE = (1 << 30),
    FIQ_SOURCE_PSRAM = (1 << 31),
};

#define FIQ_SOURCE_ALL      ( 0\
    | FIQ_SOURCE_MODEM \
    | FIQ_SOURCE_MAC_TX_RX_TIMER \
    | FIQ_SOURCE_MAC_TX_RX_MISC \
    | FIQ_SOURCE_MAC_RX_TRIGGER \
    | FIQ_SOURCE_MAC_TX_TRIGGER \
    | FIQ_SOURCE_MAC_PROT_TRIGGER \
    | FIQ_SOURCE_MAC_GENERAL \
    | FIQ_SOURCE_SDIO_DMA \
    | FIQ_SOURCE_USB_PLUG_INOUT \
    | FIQ_SOURCE_SECURITY \
    | FIQ_SOURCE_MAC_WAKE_UP \
    | FIQ_SOURCE_SPI_DMA \
    | FIQ_SOURCE_DPLL_UNLOCK \
    | FIQ_SOURCE_JPEG_ENCODER \
    | FIQ_SOURCE_BLE \
    | FIQ_SOURCE_PSRAM \
    )

typedef void interrupt_handler_cb(void);


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
