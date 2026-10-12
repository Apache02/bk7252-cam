#include "hardware/intc.h"
#include "platform/init.h"
#include "platform/cpu.h"
#include "soc/icu.h"
#include "soc/gpio.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>


#define count_of(x) (sizeof(x) / sizeof(x[0]))


#ifdef INTC_COUNT_FIRES
// Diagnostic stat

static volatile struct {
    uint32_t fire_count[32];
    uint32_t orphan_sources;
    uint16_t spurious_irq_count;
    uint16_t spurious_fiq_count;
} s_stat = {0};

static inline void count_fires(uint32_t source) {
    while (source) {
        uint32_t bit = (uint32_t)__builtin_ctz(source);

        s_stat.fire_count[bit] += 1;
        source &= source - 1;
    }
}

uint32_t intc_get_fire_count(uint8_t bit) {
    if (bit < count_of(s_stat.fire_count)) {
        return s_stat.fire_count[bit];
    }
    return 0;
}

uint32_t intc_get_spurious_irq_count() { return s_stat.spurious_irq_count; }

uint32_t intc_get_spurious_fiq_count() { return s_stat.spurious_fiq_count; }

uint32_t intc_get_orphan_sources() { return s_stat.orphan_sources; }

#define UPDATE_STAT(source, orphans)                   \
    do {                                               \
        if (orphans) s_stat.orphan_sources |= orphans; \
        count_fires(source);                           \
    } while (0)

#define ON_SPURIOUS_IRQ() s_stat.spurious_irq_count += 1
#define ON_SPURIOUS_FIQ() s_stat.spurious_fiq_count += 1

#else
#define UPDATE_STAT(source, orphans) ((void)0)
#define ON_SPURIOUS_IRQ()            ((void)0)
#define ON_SPURIOUS_FIQ()            ((void)0)
#endif


#define MAX_HANDLERS 32


struct interrupt_handler_t {
    int_handler_fn *handler;
    uint32_t        source;
};

struct handlers_collection_t {
    size_t                     count;
    uint32_t                   handled_mask;
    struct interrupt_handler_t handlers[MAX_HANDLERS];
};

static struct handlers_collection_t intc_manager = {0};

static inline void recompute_handled_mask(struct handlers_collection_t *collection) {
    uint32_t mask = 0;

    for (int i = 0; i < collection->count; i++) {
        mask |= collection->handlers[i].source;
    }

    collection->handled_mask = mask;
}

static inline void register_handler(struct handlers_collection_t *collection, uint32_t source, int_handler_fn *func) {
    assert(func != NULL);
    assert(source != 0);

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    int found = -1;
    for (int i = 0; i < collection->count; i++) {
        if (collection->handlers[i].handler == func) {
            found = i;
            break;
        }
    }

    if (found == -1) {
        assert(collection->count < MAX_HANDLERS);
        found                               = collection->count++;
        collection->handlers[found].handler = func;
        collection->handlers[found].source  = 0;
    }

    collection->handlers[found].source |= source;
    collection->handled_mask |= source;

    GLOBAL_INT_RESTORE();
}

static inline bool unregister_handler(struct handlers_collection_t *collection, uint32_t source, int_handler_fn *func) {
    int  found  = -1;
    bool delete = false;

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();

    for (int i = 0; i < collection->count; i++) {
        if (collection->handlers[i].handler == func) {
            found  = i;
            delete = (collection->handlers[i].source & ~source) == 0;
        }
    }

    if (found != -1) {
        if (delete) {
            for (int i = found; i < collection->count - 1; i++) {
                collection->handlers[i] = collection->handlers[i + 1];
            }
            collection->count--;
        } else {
            collection->handlers[found].source &= ~source;
        }

        recompute_handled_mask(collection);
    }

    GLOBAL_INT_RESTORE();

    return found != -1;
}

static int find_handlers(const struct handlers_collection_t *collection, const uint32_t source,
                         int_handler_fn **handlers, const size_t length) {
    int count = 0;

    for (int i = 0; i < collection->count; i++) {
        if ((collection->handlers[i].source & source) != 0) {
            if (count < length) {
                handlers[count] = collection->handlers[i].handler;
            }
            count++;
        }
    }

    return count;
}

static inline void process_handlers(const struct handlers_collection_t *collection, uint32_t source) {
    int_handler_fn *handlers[4];
    const int       max_count = count_of(handlers);
    const int       count     = find_handlers(collection, source, handlers, max_count);

    assert(count <= max_count);

    const int n = max_count < count ? max_count : count;
    for (int i = 0; i < n; i++) {
        handlers[i]();
    }
}


static inline void init_ram_vectors() {
    extern void do_irq(void);
    extern void do_fiq(void);
    extern void do_swi(void);
    extern void do_undefined(void);
    extern void do_pabort(void);
    extern void do_dabort(void);
    extern void do_reserved(void);

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

void intc_reset() {
    hw_icu->irq_enable.v     = 0;
    hw_icu->global_int_en.v  = 0;
    hw_icu->irq_raw_status.v = hw_icu->irq_raw_status.v;
    hw_icu->irq_status.v     = hw_icu->irq_status.v;
    hw_gpio->extra_int_cfg.v = 0;
}

INIT_AT(intc_init, 01);


void intc_irq(void) {
    hw_icu_int_t status = hw_icu->irq_status;
    status.v &= ICU_INT_IRQ_MASK;
    hw_icu->irq_status.v = status.v;

    if (!status.v) {
        ON_SPURIOUS_IRQ();
        return;
    }

    uint32_t source  = status.v;
    uint32_t orphans = source & ~intc_manager.handled_mask;
    process_handlers(&intc_manager, source);

    if (orphans) intc_disable_irq_source(orphans);

    UPDATE_STAT(source, orphans);
}

void intc_fiq(void) {
    hw_icu_int_t status = hw_icu->irq_status;
    status.v &= ICU_INT_FIQ_MASK;
    hw_icu->irq_status.v = status.v;

    if (!status.v) {
        ON_SPURIOUS_FIQ();
        return;
    }

    uint32_t source  = status.v;
    uint32_t orphans = source & ~intc_manager.handled_mask;
    process_handlers(&intc_manager, source);

    if (orphans) intc_disable_irq_source(orphans);

    UPDATE_STAT(source, orphans);
}

void intc_register_irq_handler(uint32_t source, int_handler_fn *func) { register_handler(&intc_manager, source, func); }

bool intc_unregister_irq_handler(uint32_t source, int_handler_fn *func) {
    return unregister_handler(&intc_manager, source, func);
}

void intc_enable_irq_source(uint32_t source) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    hw_icu->irq_enable.v |= source;
    GLOBAL_INT_RESTORE();
}

void intc_disable_irq_source(uint32_t source) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    hw_icu->irq_enable.v &= ~source;
    GLOBAL_INT_RESTORE();
}

bool intc_irq_source_enabled(uint32_t source) {
    const uint32_t enabled = hw_icu->irq_enable.v & source;

    return (enabled & ICU_INT_IRQ_MASK && hw_icu->global_int_en.irq) ||
           (enabled & ICU_INT_FIQ_MASK && hw_icu->global_int_en.fiq);
}
