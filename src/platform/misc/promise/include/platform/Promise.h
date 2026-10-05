#pragma once

// A one-shot handover between a producer and a consumer: the producer resolves,
// the consumer awaits, and the payload changes owner in the process.
//
// Deliberately FreeRTOS-specific, and deliberately small. Not a general future:
// no continuation, no chaining, no error type. A failure the producer wants to
// report belongs in the payload.
//
// ---- Contract -------------------------------------------------------------
//
// None of these four are checked for you, and breaking the first one hangs a
// task.
//
// 1. THE PRODUCER MUST RESOLVE, on every path — including the ones where the
//    awaited thing never happens, which is what a timer or a watchdog is for.
//    await() has no timeout, so a consumer nobody resolves is a task that never
//    runs again.
//
// 2. ONE CONSUMER, ONE SHOT. A handover, not a broadcast: it cannot wake
//    several waiters on one event.
//
// 3. THE PAYLOAD COMES FROM new, since an unawaited promise deletes it. A
//    static, a stack object or a pvPortMalloc() block is a bug the compiler
//    will not catch.
//
// 4. RESOLVE FROM THE RIGHT CONTEXT — resolve() in a task, resolve_from_isr()
//    in an interrupt.
//
// ---- Ownership ------------------------------------------------------------
//
// The promise itself is held through Shared<Promise<T>>: the producer keeps a
// handle for as long as it intends to resolve, the consumer keeps one while it
// waits, and whichever lets go last deletes it. So a consumer that walks away
// without awaiting leaks nothing and leaves nothing dangling.
//
// The payload is the other way round — one owner at a time. The promise owns it
// from resolve() until await() hands it to an Owned<T>, and not after; if
// nobody ever awaits, the destructor frees it. A resolve that loses the race
// reports false and leaves the caller owning what it passed.

#include <FreeRTOS.h>
#include <semphr.h>

#include "platform/Owned.h"
#include "platform/Shared.h"

class PromiseBase : public RefCounted {
  protected:
    PromiseBase();
    ~PromiseBase();

    PromiseBase(const PromiseBase &)            = delete;
    PromiseBase &operator=(const PromiseBase &) = delete;

    // A promise that failed to build never blocks: await_raw() answers nullptr
    // and both resolves report failure.
    bool ok() const { return sem_ != nullptr; }

    // Blocks until resolved. Clears value_ on handover, so the destructor does
    // not free what the caller now owns.
    void *await_raw();

    bool resolve_raw(void *value);
    bool resolve_raw_from_isr(void *value);

    void *value_ = nullptr;

  private:
    // A masked region a few instructions long, which is also why there is no
    // mutex here: an interrupt could not have taken one.
    bool claim(void *value);

    SemaphoreHandle_t sem_      = nullptr; // given once, on resolve
    bool              resolved_ = false;
    bool              taken_    = false;
};

template <typename T> class Promise : private PromiseBase {
  public:
    // The one part of the private base a Shared<Promise<T>> has to reach.
    using RefCounted::ref;
    using RefCounted::unref;

    Promise() = default;

    // A no-op once await() has handed the payload over.
    ~Promise() { delete static_cast<T *>(value_); }

    bool valid() const { return ok(); }

    Owned<T> await() { return Owned<T>(static_cast<T *>(await_raw())); }

    // False when the promise was already resolved, in which case `value` is
    // still the caller's to free.
    bool resolve(T *value) { return resolve_raw(value); }
    bool resolve_from_isr(T *value) { return resolve_raw_from_isr(value); }
};
