#include "shell/commands_beken.h"
#include <stdio.h>
#include "hardware/intc.h"
#include "soc/icu.h"

struct source_name {
    int         source;
    const char *name;
};

static const struct source_name irq_names[] = {
    {IRQ_SOURCE_UART1, "uart1"},
    {IRQ_SOURCE_UART2, "uart2"},
    {IRQ_SOURCE_I2C1, "i2c1"},
    {IRQ_SOURCE_IRDA, "irda"},
    {IRQ_SOURCE_I2S_PCM, "i2s"},
    {IRQ_SOURCE_I2C2, "i2c2"},
    {IRQ_SOURCE_SPI, "spi"},
    {IRQ_SOURCE_GPIO, "gpio"},
    {IRQ_SOURCE_TIMER, "timer"},
    {IRQ_SOURCE_PWM, "pwm"},
    {IRQ_SOURCE_AUDIO, "audio"},
    {IRQ_SOURCE_SARADC, "saradc"},
    {IRQ_SOURCE_SDIO, "sdio"},
    {IRQ_SOURCE_USB, "usb"},
    {IRQ_SOURCE_FFT, "fft"},
    {IRQ_SOURCE_GDMA, "gdma"},
    {0, nullptr},
};

static const struct source_name fiq_names[] = {
    {FIQ_SOURCE_MODEM, "modem"},
    {FIQ_SOURCE_MAC_TX_RX_TIMER, "mac_tx_rx_timer"},
    {FIQ_SOURCE_MAC_TX_RX_MISC, "mac_tx_rx_misc"},
    {FIQ_SOURCE_MAC_RX_TRIGGER, "mac_rx_trigger"},
    {FIQ_SOURCE_MAC_TX_TRIGGER, "mac_tx_trigger"},
    {FIQ_SOURCE_MAC_PROT_TRIGGER, "mac_prot_trigger"},
    {FIQ_SOURCE_MAC_GENERAL, "mac_general"},
    {FIQ_SOURCE_SDIO_DMA, "sdio_dma"},
    {FIQ_SOURCE_USB_PLUG_INOUT, "usb_plug"},
    {FIQ_SOURCE_SECURITY, "security"},
    {FIQ_SOURCE_MAC_WAKE_UP, "mac_wake_up"},
    {FIQ_SOURCE_SPI_DMA, "spi_dma"},
    {FIQ_SOURCE_DPLL_UNLOCK, "dpll_unlock"},
    {FIQ_SOURCE_JPEG_ENCODER, "jpeg_encoder"},
    {FIQ_SOURCE_BLE, "ble"},
    {FIQ_SOURCE_PSRAM, "psram"},
    {0, nullptr},
};

static void print_sources(const char *label, const struct source_name *names, uint32_t mask) {
    printf("%s 0x%08lx", label, mask);

    if (mask) {
        const char *separator = " ";
        for (int i = 0; names[i].name != nullptr; i++) {
            if ((names[i].source & mask) == 0) continue;
            printf("%s%s", separator, names[i].name);
            separator = ",";
        }
    }

    printf("\r\n");
}

int command_intc(__unused int argc, __unused const char *argv[]) {
    uint32_t enable = hw_icu->irq_enable.v;
    uint32_t raw    = hw_icu->irq_raw_status.v;

    printf("global: irq=%d fiq=%d\r\n", (int)hw_icu->global_int_en.irq, (int)hw_icu->global_int_en.fiq);

    print_sources("irq enabled: ", irq_names, enable & ICU_INT_IRQ_MASK);
    print_sources("irq raw:     ", irq_names, raw & ICU_INT_IRQ_MASK);
    print_sources("irq orphan:  ", irq_names, intc_orphan_irq_sources);
    printf("irq spurious: %lu\r\n", intc_spurious_irq_count);

    printf("\r\n");

    print_sources("fiq enabled: ", fiq_names, enable & ICU_INT_FIQ_MASK);
    print_sources("fiq raw:     ", fiq_names, raw & ICU_INT_FIQ_MASK);
    print_sources("fiq orphan:  ", fiq_names, intc_orphan_fiq_sources);
    printf("fiq spurious: %lu\r\n", intc_spurious_fiq_count);

    return 0;
}
