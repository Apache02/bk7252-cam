#include "hardware/intc.h"
#include "platform/init.h"
#include <stdio.h>
#include <string.h>

#include "soc/icu.h"
#include "intc_manager.h"
#include "soc/gpio.h"


extern void do_irq(void);
extern void do_fiq(void);
extern void do_swi(void);
extern void do_undefined(void);
extern void do_pabort(void);
extern void do_dabort(void);
extern void do_reserved(void);


static inline void init_ram_vectors() {
    ram_vectors->irq       = do_irq;
    ram_vectors->fiq       = do_fiq;
    ram_vectors->swi       = do_swi;
    ram_vectors->undefined = do_undefined;
    ram_vectors->pabort    = do_pabort;
    ram_vectors->dabort    = do_dabort;
    ram_vectors->reserved  = do_reserved;
}

static void intc_init(void) {
    memset(&intc_manager, 0, sizeof(intc_manager));

    init_ram_vectors();

    // No source is unmasked here. A source is only forwarded to the core once a
    // handler is registered for it (intc_register_irq_handler callers pair the
    // registration with intc_enable_irq_source). Unmasking a source with no
    // handler is a hard hang: nothing acknowledges the peripheral, so the ICU
    // re-latches it the instant intc_fiq() returns and the core never leaves
    // exception context again.

    // One store, not two bitfield read-modify-writes: the second would otherwise
    // read back a register whose IRQ line it had just opened.
    hw_write_fields(hw_icu->global_int_en,
        .irq = 1,
        .fiq = 1,
    );
}

INIT_AT(intc_init, 01);

// Sources that fired with no handler registered, masked at the ICU so they don't
// storm forever. Latched here so the cause stays visible.
volatile uint32_t intc_orphan_irq_sources;

// Exceptions taken with the ICU status already empty — the source deasserted
// before this handler read it. Benign on this ICU; just counted.
volatile uint32_t intc_spurious_irq_count;
volatile uint32_t intc_spurious_fiq_count;

#ifdef INTC_COUNT_FIRES
// Per-source fire counts, indexed by the same bit position as IRQ_SOURCE_*
// (0-31). Counts every source bit the dispatcher decodes off the ICU status
// register in intc_irq()/intc_fiq(), before dispatch to any
// handler — so a source fires here even if nothing is registered for it (an
// orphan) or intc_service_register() was never involved. Diagnostic only;
// define INTC_COUNT_FIRES to build it in.
static volatile uint32_t s_fire_count[32];

static inline void count_fires(uint32_t source) {
    while (source) {
        uint32_t bit = (uint32_t)__builtin_ctz(source);
        s_fire_count[bit]++;
        source &= source - 1;
    }
}

uint32_t intc_get_fire_count(uint8_t bit) { return (bit < 32) ? s_fire_count[bit] : 0; }
#endif // INTC_COUNT_FIRES


void intc_irq(void) {
    hw_icu_int_t status = hw_icu->irq_status;

    status.v &= ICU_INT_IRQ_MASK;
    if (!status.v) {
        intc_spurious_irq_count++;
        return;
    }

    hw_icu->irq_status.v = status.v;
    uint32_t source = status.v;

    uint32_t orphans = source & ~intc_manager.handled_mask;

    process_handlers(&intc_manager, source);

    if (orphans) {
        intc_orphan_irq_sources |= orphans;
        intc_disable_irq_source(orphans);
    }

#ifdef INTC_COUNT_FIRES
    count_fires(source);
#endif
}

void intc_fiq(void) {
    hw_icu_int_t status = hw_icu->irq_status;

    status.v &= ICU_INT_FIQ_MASK;
    if (!status.v) {
        intc_spurious_fiq_count++;
        return;
    }

    hw_icu->irq_status.v = status.v;
    uint32_t source = status.v;

    uint32_t orphans = source & ~intc_manager.handled_mask;

    process_handlers(&intc_manager, source);

    if (orphans) {
        intc_orphan_irq_sources |= orphans;
        intc_disable_irq_source(orphans);
    }

#ifdef INTC_COUNT_FIRES
    count_fires(source);
#endif
}

bool intc_register_irq_handler(uint32_t source, int_handler_fn *func) {
    if (intc_manager.count >= MAX_HANDLERS) return false;
    return register_handler(&intc_manager, source, func);
}

bool intc_unregister_irq_handler(uint32_t source, int_handler_fn *func) {
    return unregister_handler(&intc_manager, source, func);
}

void intc_enable_irq_source(uint32_t source) {
    disable_interrupts();
    hw_icu->irq_enable.v |= source;
    restore_interrupts();
}

void intc_disable_irq_source(uint32_t source) {
    disable_interrupts();
    hw_icu->irq_enable.v &= ~source;
    restore_interrupts();
}

// A source's bit position also says which core line the ICU forwards it on
// (see ICU_INT_IRQ_MASK/ICU_INT_FIQ_MASK) — mixing bits from both halves in one
// call is not supported.
bool intc_irq_source_enabled(uint32_t source) {
    if ((hw_icu->irq_enable.v & source) == 0) return false;
    return (source & ICU_INT_IRQ_MASK) ? !!hw_icu->global_int_en.irq : !!hw_icu->global_int_en.fiq;
}

void intc_reset() {
    hw_icu->irq_enable.v     = 0;
    hw_icu->global_int_en.v  = 0;
    hw_icu->irq_raw_status.v = hw_icu->irq_raw_status.v;
    hw_icu->irq_status.v     = hw_icu->irq_status.v;
    hw_gpio->extra_int_cfg.v = 0;
}
