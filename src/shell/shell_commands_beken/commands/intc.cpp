#include "shell/commands_beken.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "hardware/intc.h"
#include "soc/icu.h"

// A source is identified by its bit mask (IRQ_SOURCE_* in bits 0-15, FIQ_SOURCE_* in 16-31).
static const char *source_name(const int source) {
    switch (source) {
        case IRQ_SOURCE_UART1: return "UART1";
        case IRQ_SOURCE_UART2: return "UART2";
        case IRQ_SOURCE_I2C1: return "I2C1";
        case IRQ_SOURCE_IRDA: return "IRDA";
        case IRQ_SOURCE_I2S_PCM: return "I2S_PCM";
        case IRQ_SOURCE_I2C2: return "I2C2";
        case IRQ_SOURCE_SPI: return "SPI";
        case IRQ_SOURCE_GPIO: return "GPIO";
        case IRQ_SOURCE_TIMER: return "TIMER";
        case IRQ_SOURCE_PWM: return "PWM";
        case IRQ_SOURCE_AUDIO: return "AUDIO";
        case IRQ_SOURCE_SARADC: return "SARADC";
        case IRQ_SOURCE_SDIO: return "SDIO";
        case IRQ_SOURCE_USB: return "USB";
        case IRQ_SOURCE_FFT: return "FFT";
        case IRQ_SOURCE_GDMA: return "GDMA";
        case FIQ_SOURCE_MODEM: return "MODEM";
        case FIQ_SOURCE_MAC_TX_RX_TIMER: return "MAC_TX_RX_TIMER";
        case FIQ_SOURCE_MAC_TX_RX_MISC: return "MAC_TX_RX_MISC";
        case FIQ_SOURCE_MAC_RX_TRIGGER: return "MAC_RX_TRIGGER";
        case FIQ_SOURCE_MAC_TX_TRIGGER: return "MAC_TX_TRIGGER";
        case FIQ_SOURCE_MAC_PROT_TRIGGER: return "MAC_PROT_TRIGGER";
        case FIQ_SOURCE_MAC_GENERAL: return "MAC_GENERAL";
        case FIQ_SOURCE_SDIO_DMA: return "SDIO_DMA";
        case FIQ_SOURCE_USB_PLUG_INOUT: return "USB_PLUG_INOUT";
        case FIQ_SOURCE_SECURITY: return "SECURITY";
        case FIQ_SOURCE_MAC_WAKE_UP: return "MAC_WAKE_UP";
        case FIQ_SOURCE_SPI_DMA: return "SPI_DMA";
        case FIQ_SOURCE_DPLL_UNLOCK: return "DPLL_UNLOCK";
        case FIQ_SOURCE_JPEG_ENCODER: return "JPEG_ENCODER";
        case FIQ_SOURCE_BLE: return "BLE";
        case FIQ_SOURCE_PSRAM: return "PSRAM";
        default: return nullptr;
    }
}

static void print_sources(const char *label, uint32_t mask) {
    printf("%s 0x%08lx", label, mask);

    if (mask) {
        auto separator = " ";
        for (int bit = 0; bit < 32; bit++) {
            uint32_t    source = mask & (1u << bit);
            const char *name   = source ? source_name(static_cast<int>(source)) : nullptr;
            if (!name) continue;
            printf("%s%s", separator, name);
            separator = ", ";
        }
    }

    printf("\r\n");
}

#ifdef INTC_COUNT_FIRES
// int_num is the bit index, not the mask.
static const char *source_name_by_bit(int bit) { return source_name(static_cast<int>(1u << bit)); }

#define MAC_SOURCE_GROUP                                                                      \
    (0 | FIQ_SOURCE_MAC_TX_RX_TIMER | FIQ_SOURCE_MAC_TX_RX_MISC | FIQ_SOURCE_MAC_RX_TRIGGER | \
     FIQ_SOURCE_MAC_TX_TRIGGER | FIQ_SOURCE_MAC_PROT_TRIGGER | FIQ_SOURCE_MAC_GENERAL | FIQ_SOURCE_MAC_WAKE_UP)

// clang-format off
static const struct {
    const char *name;
    uint32_t    mask;
} s_groups[] = {
    {"irq", static_cast<uint32_t>(IRQ_SOURCE_ALL)},
    {"fiq", static_cast<uint32_t>(FIQ_SOURCE_ALL)},
    {"mac", MAC_SOURCE_GROUP},
    {"uart", (IRQ_SOURCE_UART1 | IRQ_SOURCE_UART2)},
    {"i2c", (IRQ_SOURCE_I2C1 | IRQ_SOURCE_I2C2)},
};
// clang-format on

static bool streq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower(static_cast<unsigned char>(*a)) != tolower(static_cast<unsigned char>(*b))) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static void print_fire_count(int bit) {
    printf("  %2d %-17s %lu\r\n", bit, source_name_by_bit(bit),
           static_cast<unsigned long>(intc_get_fire_count(static_cast<uint8_t>(bit))));
}

static void print_fire_mask(uint32_t mask) {
    for (int bit = 0; bit < 32; bit++) {
        if (mask & (1u << bit)) print_fire_count(bit);
    }
}

// Resolves one argument to a source/group and prints it. Accepts, in order:
// a named group ("irq", "fiq", "mac", "uart", "i2c"), a bit index ("20"), or
// an exact source name ("mac_tx_trigger") — all case-insensitive.
static void print_fire_arg(const char *arg) {
    for (size_t g = 0; g < sizeof(s_groups) / sizeof(s_groups[0]); g++) {
        if (streq_ci(arg, s_groups[g].name)) {
            print_fire_mask(s_groups[g].mask);
            return;
        }
    }

    char         *end = nullptr;
    unsigned long bit = strtoul(arg, &end, 0);
    if (end != arg && *end == '\0' && bit < 32) {
        print_fire_count(static_cast<int>(bit));
        return;
    }

    for (int i = 0; i < 32; i++) {
        const char *name = source_name_by_bit(i);
        if (name && streq_ci(arg, name)) {
            print_fire_count(i);
            return;
        }
    }

    printf("intc: unknown source/group '%s'\r\n", arg);
}
#endif // INTC_COUNT_FIRES

// Interrupt controller state: enabled/raw sources, sources that fired without a
// handler, the count of exceptions taken with an empty status, and — behind
// INTC_COUNT_FIRES — per-source fire counts since boot (hardware_intc/intc.c),
// used to check whether the MAC hardware ever attempts a real TX/RX
// (int_num 17-22) independent of what's observed over the air.
//
// usage: intc [source_or_group ...]
//   no args      — full state, plus every fire count that is nonzero
//   bit index    — e.g. `intc 20`
//   source name  — e.g. `intc mac_tx_trigger`
//   group name   — `intc irq`, `fiq`, `mac`, `uart`, `i2c`
int command_intc(int argc, const char *argv[]) {
    uint32_t enable = hw_icu->irq_enable.v;
    uint32_t raw    = hw_icu->irq_raw_status.v;

    printf("global: irq=%d fiq=%d\r\n\r\n", static_cast<int>(hw_icu->global_int_en.irq), static_cast<int>(hw_icu->global_int_en.fiq));

    print_sources("irq enabled: ", enable & ICU_INT_IRQ_MASK);
    print_sources("irq raw:     ", raw & ICU_INT_IRQ_MASK);
#ifdef INTC_COUNT_FIRES
    print_sources("irq orphan:  ", intc_get_orphan_sources() & ICU_INT_IRQ_MASK);
    printf("irq spurious: %lu\r\n", intc_get_spurious_irq_count());
#endif

    printf("\r\n");

    print_sources("fiq enabled: ", enable & ICU_INT_FIQ_MASK);
    print_sources("fiq raw:     ", raw & ICU_INT_FIQ_MASK);
#ifdef INTC_COUNT_FIRES
    print_sources("fiq orphan:  ", intc_get_orphan_sources() & ICU_INT_FIQ_MASK);
    printf("fiq spurious: %lu\r\n", intc_get_spurious_fiq_count());
#endif

    printf("\r\n");

#ifdef INTC_COUNT_FIRES
    if (argc < 2) {
        for (int i = 0; i < 32; i++) {
            if (intc_get_fire_count(static_cast<uint8_t>(i)) > 0) print_fire_count(i);
        }
    } else {
        for (int a = 1; a < argc; a++) {
            print_fire_arg(argv[a]);
        }
    }
#else
    printf("orphan, spurious and fire counts: built without INTC_COUNT_FIRES\r\n");
#endif

    return 0;
}
