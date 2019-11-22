#pragma once
#include <queue>
#include <mutex>
#include <memory>
#include "spinlock.h"
template<typename T> class ReadWriteQueue {
public:
    ReadWriteQueue() {}
    ~ReadWriteQueue() {}

    void Push(T new_value) {
        lock_.lock();
        queue_.push(std::move(new_value));
        lock_.unlock();
    }

    bool TryPop(T& value) {
        std::lock_guard<SpinLock> lk(lock_);
        if(queue_.empty())
            return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
  }
private:
    SpinLock lock_;
    std::queue<T> queue_;
};
