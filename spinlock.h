#pragma once
#include <atomic>
class SpinLock {
public:
    SpinLock() : lock_(ATOMIC_FLAG_INIT) {}
    ~SpinLock() = default;
    /*!
     * \brief Acquire lock.
     */
    inline void lock() noexcept(true) {
        while (lock_.test_and_set(std::memory_order_acquire)) {
        }
    }
    /*!
     * \brief Release lock.
     */
    inline void unlock() noexcept(true) {
        lock_.clear(std::memory_order_release);
    }
private:
    std::atomic_flag lock_;
    SpinLock(const SpinLock&) = delete;
};
