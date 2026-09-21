#pragma once

// Shared ownership without <memory>, and without atomics this CPU does not
// have: ARMv5 has no LDREX/STREX, and libstdc++ answers that by counting
// std::shared_ptr's references non-atomically, which is wrong the moment two
// tasks hold one.
//
// The count lives inside the object instead of in a control block, so a shared
// object costs four bytes and no second allocation.

#include <stdint.h>

class RefCounted {
  public:
    void ref();

    // True when the caller let go of the last reference and now owes the object
    // a delete.
    bool unref();

  protected:
    // Born holding one reference, which the first handle adopts rather than
    // adds to.
    RefCounted()  = default;
    ~RefCounted() = default;

    RefCounted(const RefCounted &)            = delete;
    RefCounted &operator=(const RefCounted &) = delete;

  private:
    uint32_t refs_ = 1;
};

// A handle to a RefCounted object; the last handle to go deletes it.
//
// Where Owned<T> has one owner and moves, this one copies, which is the whole
// point: a producer and a consumer can each hold a handle, and neither has to
// outlive the other.
//
// Let a handle go from a task, never from an interrupt — the last one calls
// delete, and delete calls free().

template <typename T> class Shared {
  public:
    constexpr Shared() = default;

    explicit Shared(T *value) : value_(value) {}

    ~Shared() { drop(); }

    Shared(const Shared &other) : value_(other.value_) {
        if (value_) value_->ref();
    }

    Shared &operator=(const Shared &other) {
        // Counted up before the old one is dropped, so assigning a handle to
        // itself cannot free what it holds.
        if (other.value_) other.value_->ref();
        drop();
        value_ = other.value_;
        return *this;
    }

    Shared(Shared &&other) : value_(other.value_) { other.value_ = nullptr; }

    Shared &operator=(Shared &&other) {
        if (this != &other) {
            drop();
            value_       = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }

    T *operator->() const { return value_; }
    T &operator*() const { return *value_; }

    // The only way to test a handle: there is deliberately no conversion to
    // bool, so nothing about one can be tested by accident.
    bool empty() const { return value_ == nullptr; }

    T *get() const { return value_; }

    // Hands the reference to a new handle and empties this one — a move that
    // does not need <utility>.
    Shared take() {
        Shared handle;
        handle.value_ = value_;
        value_        = nullptr;
        return handle;
    }

    // Lets the reference go early, which is what a producer does once it has
    // nothing left to resolve.
    void clear() {
        drop();
        value_ = nullptr;
    }

  private:
    void drop() {
        if (value_ && value_->unref()) delete value_;
    }

    T *value_ = nullptr;
};
