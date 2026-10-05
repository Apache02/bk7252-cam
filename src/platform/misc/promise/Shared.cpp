#include "platform/Shared.h"

#include "platform/cpu.h"

// A masked region two instructions long. The same reason PromiseBase::claim()
// masks rather than takes a mutex: an interrupt could not have taken one.

void RefCounted::ref() {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    refs_++;
    GLOBAL_INT_RESTORE();
}

bool RefCounted::unref() {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    bool last = --refs_ == 0;
    GLOBAL_INT_RESTORE();
    return last;
}
