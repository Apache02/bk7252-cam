#ifndef _HARDWARE_INTC_H
#define _HARDWARE_INTC_H

#include <stdbool.h>
#include <stdint.h>

typedef void(int_handler_fn)(void);

typedef struct {
    int_handler_fn *irq;
    int_handler_fn *fiq;
    int_handler_fn *swi;
    int_handler_fn *undefined;
    int_handler_fn *pabort;
    int_handler_fn *dabort;
    int_handler_fn *reserved;
    uint32_t        start_type;
} ram_vectors_tbl_t;

#define ram_vectors ((volatile ram_vectors_tbl_t *)0x00400000)

#ifdef __cplusplus
extern "C" {
#endif

void intc_reset();

bool intc_register_irq_handler(uint32_t source, int_handler_fn *func);

bool intc_unregister_irq_handler(uint32_t source, int_handler_fn *func);

void intc_enable_irq_source(uint32_t source);

void intc_disable_irq_source(uint32_t source);

// Read-only: true if at least one source in the mask is unmasked at the ICU and has its
// global IRQ or FIQ line open, so it can reach the core. Use it before WFI to check that
// something can wake the core. Ignores the CPSR I/F bits and whether a handler is registered.
bool intc_irq_source_enabled(uint32_t source);

#ifdef INTC_COUNT_FIRES
// Diagnostic only (define INTC_COUNT_FIRES to build it in): how many times
// source bit `bit` (an IRQ_SOURCE_* bit position, 0-31) has been decoded off
// the ICU status register by intc_irq()/intc_fiq(), regardless of whether a
// handler was registered for it.
uint32_t intc_get_fire_count(uint8_t bit);

// Exceptions taken with an empty ICU status: the source deasserted before the dispatcher
// read it. Expected on this ICU; counted, not fatal.
uint32_t intc_get_spurious_irq_count();
uint32_t intc_get_spurious_fiq_count();

// Sources that fired with no handler registered, as a bit mask. The dispatcher masks such a
// source at the ICU, because nothing can acknowledge it and it would storm. Non-zero means
// a peripheral was unmasked without its driver being wired up.
uint32_t intc_get_orphan_sources();
#endif

#ifdef __cplusplus
}
#endif

#endif // _HARDWARE_INTC_H
