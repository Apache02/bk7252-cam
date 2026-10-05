#pragma once

// A stack handle that owns one heap object and deletes it when the scope ends —
// including a scope left through an early return, which is the point of it.
//
// Moving hands the object to the new handle and empties the old one, so exactly
// one destructor ever frees it. Copying would leave two handles believing they
// own the same object, so there is no copy.

template <typename T> class Owned {
  public:
    Owned() = default;
    explicit Owned(T *value) : value_(value) {}

    ~Owned() { delete value_; }

    Owned(Owned &&other) : value_(other.value_) { other.value_ = nullptr; }

    Owned &operator=(Owned &&other) {
        if (this != &other) {
            delete value_;
            value_       = other.value_;
            other.value_ = nullptr;
        }
        return *this;
    }

    Owned(const Owned &)            = delete;
    Owned &operator=(const Owned &) = delete;

    T *operator->() const { return value_; }
    T &operator*() const { return *value_; }

    // The only way to test a handle: there is deliberately no conversion to
    // bool, so nothing about one can be tested by accident.
    bool empty() const { return value_ == nullptr; }

    // Borrows the object without giving it up; the handle still frees it.
    T *get() const { return value_; }

    // Gives the object up to a caller that takes over freeing it.
    T *release() {
        T *value = value_;
        value_   = nullptr;
        return value;
    }

  private:
    T *value_ = nullptr;
};
