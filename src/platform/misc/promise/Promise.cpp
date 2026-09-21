#include "platform/Promise.h"

#include "platform/cpu.h"

// Untemplated, so a second and third Promise<T> cost a cast rather than another
// copy of the logic.

PromiseBase::PromiseBase() { sem_ = xSemaphoreCreateBinary(); }

PromiseBase::~PromiseBase() {
    if (sem_) vSemaphoreDelete(sem_);
}

bool PromiseBase::claim(void *value) {
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    bool first = !resolved_;
    if (first) {
        value_    = value;
        resolved_ = true;
    }
    GLOBAL_INT_RESTORE();
    return first;
}

void *PromiseBase::await_raw() {
    if (!ok()) return nullptr;

    xSemaphoreTake(sem_, portMAX_DELAY);

    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();
    void *value = taken_ ? nullptr : value_;
    taken_      = true;
    value_      = nullptr;
    GLOBAL_INT_RESTORE();

    // Put it back so a second await() answers nullptr instead of blocking.
    xSemaphoreGive(sem_);

    return value;
}

bool PromiseBase::resolve_raw(void *value) {
    if (!ok() || !claim(value)) return false;

    xSemaphoreGive(sem_);
    return true;
}

bool PromiseBase::resolve_raw_from_isr(void *value) {
    if (!ok() || !claim(value)) return false;

    // Must start false: FreeRTOS only ever *sets* it, so an uninitialised value
    // yields from every ISR that woke nobody.
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(sem_, &woken);
    portYIELD_FROM_ISR(woken);
    return true;
}
