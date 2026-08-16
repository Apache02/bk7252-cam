#include "shell/commands_beken.h"
#include "hardware/intc.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>


#ifdef INTC_COUNT_FIRES
// Names from hardware/intc.h's IRQ_SOURCE_*/FIQ_SOURCE_* bit assignments —
// int_num is the bit index, not the enum value.
static const char *const s_int_names[32] = {
    "UART1",
    "UART2",
    "I2C1",
    "IRDA",
    "I2S_PCM",
    "I2C2",
    "SPI",
    "GPIO",
    "TIMER",
    "PWM",
    "AUDIO",
    "SARADC",
    "SDIO",
    "USB",
    "FFT",
    "GDMA",
    "MODEM",
    "MAC_TX_RX_TIMER",
    "MAC_TX_RX_MISC",
    "MAC_RX_TRIGGER",
    "MAC_TX_TRIGGER",
    "MAC_PROT_TRIGGER",
    "MAC_GENERAL",
    "SDIO_DMA",
    "USB_PLUG_INOUT",
    "SECURITY",
    "MAC_WAKE_UP",
    "SPI_DMA",
    "DPLL_UNLOCK",
    "JPEG_ENCODER",
    "BLE",
    "PSRAM",
};

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

static void print_source(int bit) {
    printf("  %2d %-17s %lu\r\n", bit, s_int_names[bit],
           static_cast<unsigned long>(intc_get_fire_count(static_cast<uint8_t>(bit))));
}

static void print_mask(uint32_t mask) {
    for (int bit = 0; bit < 32; bit++) {
        if (mask & (1u << bit)) print_source(bit);
    }
}

// Resolves one argument to a source/group and prints it. Accepts, in order:
// a named group ("irq", "fiq", "mac", "uart", "i2c"), a bit index ("20"), or
// an exact source name ("mac_tx_trigger") — all case-insensitive.
static void print_arg(const char *arg) {
    for (size_t g = 0; g < sizeof(s_groups) / sizeof(s_groups[0]); g++) {
        if (streq_ci(arg, s_groups[g].name)) {
            print_mask(s_groups[g].mask);
            return;
        }
    }

    char         *end = nullptr;
    unsigned long bit = strtoul(arg, &end, 0);
    if (end != arg && *end == '\0' && bit < 32) {
        print_source(static_cast<int>(bit));
        return;
    }

    for (int i = 0; i < 32; i++) {
        if (streq_ci(arg, s_int_names[i])) {
            print_source(i);
            return;
        }
    }

    printf("irq_counters: unknown source/group '%s'\r\n", arg);
}
#endif // INTC_COUNT_FIRES

// Dumps how many times each ICU source bit has fired since boot (hardware_intc/
// intc.c, behind INTC_COUNT_FIRES) — used to check whether the MAC hardware
// ever attempts a real TX/RX (int_num 17-22) independent of what's observed
// over the air.
//
// usage: irq_counters [source_or_group ...]
//   no args      — every source with a nonzero count
//   bit index    — e.g. `irq_counters 20`
//   source name  — e.g. `irq_counters mac_tx_trigger`
//   group name   — `irq`, `fiq`, `mac`, `uart`, `i2c`
int command_irq_counters(__unused int argc, __unused const char *argv[]) {
#ifdef INTC_COUNT_FIRES
    if (argc < 2) {
        for (int i = 0; i < 32; i++) {
            if (intc_get_fire_count(static_cast<uint8_t>(i)) > 0) print_source(i);
        }
        return 0;
    }

    for (int a = 1; a < argc; a++) {
        print_arg(argv[a]);
    }
#else
    printf("irq_counters: built without INTC_COUNT_FIRES\r\n");
#endif
    return 0;
}
