#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include "platform/cpu.h"


#define count_of(x) (sizeof(x) / sizeof(x[0]))

#define disable_interrupts()  \
    GLOBAL_INT_DECLARATION(); \
    GLOBAL_INT_DISABLE()
#define restore_interrupts() GLOBAL_INT_RESTORE()

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

static struct handlers_collection_t intc_manager;

static inline void recompute_handled_mask(struct handlers_collection_t *collection) {
    uint32_t mask = 0;

    for (int i = 0; i < collection->count; i++) {
        mask |= collection->handlers[i].source;
    }

    collection->handled_mask = mask;
}

static inline bool register_handler(struct handlers_collection_t *collection, uint32_t source, int_handler_fn *func) {
    disable_interrupts();

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

    restore_interrupts();
    return true;
}

static inline bool unregister_handler(struct handlers_collection_t *collection, uint32_t source, int_handler_fn *func) {
    int found   = -1;
    bool delete = false;

    disable_interrupts();

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

    restore_interrupts();

    return found != -1;
}

static int find_handlers(const struct handlers_collection_t *collection, const uint32_t source,
                         int_handler_fn **handlers, const size_t length) {
    int count = 0;

    for (int i = 0; i < collection->count; i++) {
        assert(count < length);

        if ((collection->handlers[i].source & source) != 0) {
            handlers[count] = collection->handlers[i].handler;
            count++;
        }
    }

    return count;
}

static inline void process_handlers(const struct handlers_collection_t *collection, uint32_t source) {
    int_handler_fn *handlers[4];
    int             count = find_handlers(collection, source, handlers, count_of(handlers));

    for (int i = 0; i < count; i++) {
        handlers[i]();
    }
}
