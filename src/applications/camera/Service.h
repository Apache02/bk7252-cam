#pragma once

// A service built on a C++20 coroutine. It never runs by itself: the owner calls poll(), and the
// body runs until the next co_await. All services share the stack of the task that polls them.
//
//     Service ap = service_ap(config);
//     while (!ap.done()) { ap.poll(); vTaskDelay(1000); }

#include <coroutine>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

class Service {
  public:
    struct promise_type {
        bool       result   = false;
        bool       sleeping = false;
        TickType_t wake_at  = 0;

        Service get_return_object() { return Service(std::coroutine_handle<promise_type>::from_promise(*this)); }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void                return_value(bool value) { result = value; }
        void                unhandled_exception() {}
    };

    Service() = default;
    Service(Service &&other) noexcept : m_handle(other.m_handle) { other.m_handle = nullptr; }
    Service &operator=(Service &&other) noexcept {
        if (this != &other) {
            reset();
            m_handle       = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }
    Service(const Service &)            = delete;
    Service &operator=(const Service &) = delete;
    ~Service() { reset(); }

    explicit operator bool() const { return m_handle != nullptr; }

    bool done() const { return m_handle && m_handle.done(); }

    // The value of co_return. Valid once done() is true.
    bool result() const { return m_handle.promise().result; }

    // Runs the body until the next co_await. Does nothing for an empty or finished service, or while
    // it sleeps in sleep_ms().
    void poll() const {
        if (!m_handle || m_handle.done()) return;

        promise_type &promise = m_handle.promise();
        if (promise.sleeping) {
            if (static_cast<int32_t>(xTaskGetTickCount() - promise.wake_at) < 0) return;
            promise.sleeping = false;
        }
        m_handle.resume();
    }

    // Destroys the coroutine frame and leaves the service empty.
    void reset() {
        if (m_handle) m_handle.destroy();
        m_handle = nullptr;
    }

    bool empty() const { return m_handle == nullptr; }

  private:
    explicit Service(std::coroutine_handle<promise_type> handle) : m_handle(handle) {}

    std::coroutine_handle<promise_type> m_handle;
};

// `co_await yield_now()` hands control back to the owner. The body goes on at the next poll().
inline std::suspend_always yield_now() { return {}; }

// `co_await sleep_ms(n)` hands control back like yield_now(), and poll() skips the service until at
// least n milliseconds of ticks have passed.
struct SleepAwaiter {
    uint32_t ms;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<Service::promise_type> handle) const noexcept {
        Service::promise_type &promise = handle.promise();
        promise.wake_at                = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
        promise.sleeping               = true;
    }
    void await_resume() const noexcept {}
};

inline SleepAwaiter sleep_ms(const uint32_t ms) { return {ms}; }
