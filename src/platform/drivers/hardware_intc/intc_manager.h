#pragma once

#include <stdint.h>
#include <stdbool.h>


// TODO: write macros
#define disable_interrupts()
#define restore_interrupts()

// max interrupts handlers
#define MAX_HANDLERS        32


struct interrupt_handler_t {
    interrupt_handler_cb *handler;
    uint32_t source;
};

struct handlers_collection_t {
    size_t count;
    struct interrupt_handler_t handlers[MAX_HANDLERS];
};

static struct {
    struct handlers_collection_t irq;
    struct handlers_collection_t fiq;
} intc_manager;


static inline bool
register_handler(struct handlers_collection_t *collection, uint32_t source, interrupt_handler_cb *func) {
    disable_interrupts();

    for (int i = 0; i < collection->count; i++) {
        if (collection->handlers[i].handler == func) {
            collection->handlers[i].source |= source;
            return true;
        }
    }

    int i = collection->count;
    collection->handlers[i].handler = func;
    collection->handlers[i].source = source;
    collection->count++;

    restore_interrupts();
    return true;
}

static inline bool
unregister_handler(struct handlers_collection_t *collection, uint32_t source, interrupt_handler_cb *func) {
    int found = -1;
    bool delete = false;

    disable_interrupts();

    for (int i = 0; i < collection->count; i++) {
        if (collection->handlers[i].handler == func) {
            found = i;
            delete = (collection->handlers[i].source & ~source) == 0;
        }
    }

    if (found != -1) {
        if (delete) {
            for (int i = found; i < collection->count - 1; i++) {
                collection->handlers[i] = collection->handlers[i + 1];
            }
        } else {
            collection->handlers[found].source &= ~source;
        }
    }

    restore_interrupts();

    return found != -1;
}

