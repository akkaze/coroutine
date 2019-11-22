#pragma once
#include <iostream>
#include <thread>
#include <atomic>

#include "smallvector.h"
#include "spinlock.h"
#include "readwrite_queue.h"
#include "coroutine.h"

namespace coro {

using Task = std::function<void()>;
using TaskQueue = ReadWriteQueue<Task>;

class Processor {
private:
    std::vector<routine_t> workers_;
    TaskQueue& task_queue_;
    Channel<Task> tasks_;
    std::atomic<bool>& stop_;
    uint64_t num_workers_;
public:
    Processor(const uint64_t& num_workers, TaskQueue& task_queue,
            std::atomic<bool>& stop):
            num_workers_(num_workers), task_queue_(task_queue), stop_(stop) {
        for (auto i = 0U; i < num_workers_; i++) {
            const auto& worker = Create(std::bind(
                    &Processor::ConsumeTask, this));
            workers_.push_back(worker);
        }
    }
    ~Processor() {
    }

    void ConsumeTask() {
        do {
            Task task;
            if (tasks_.Pop(task)) {
                task();
            }
        } while (!(stop_.load(std::memory_order_acquire) &&
                    tasks_.IsEmpty()));
    }

    void Run() {
        bool work_done = false;
        while (!work_done) {
            work_done = true;
            for (const auto& worker : workers_) {
                Task task;
                if (task_queue_.TryPop(task)) {
                    work_done = false;
                    tasks_.Push(task);
                }
                auto ret = Resume(worker);
                if (ret != -2) {
                    work_done = false;
                }
                if (stop_.load(std::memory_order_acquire) && work_done) {
                    if (tasks_.IsEmpty())
                    tasks_.Close();
                }
            }
        }
        for (const auto& worker : workers_) {
            Destroy(worker);
        }
    }

    void Finalize() {
        stop_.store(true, std::memory_order_release);
    }

    void AddTask(const Task& task) {
        task_queue_.Push(task);
    }
};

class ProcessorPool {
public:
    ProcessorPool() = delete;
    ProcessorPool(const uint64_t num_workers_per_core):
            ProcessorPool(std::thread::hardware_concurrency(),
            num_workers_per_core) {}
    ProcessorPool(const uint64_t& num_cores, const uint64_t&
            num_workers_per_core) : num_cores_(num_cores),
            num_workers_per_core_(num_workers_per_core),
            last_core_(0U) {
        for (auto core = 0U; core < num_cores; core++) {
            task_queues_.EmplaceBack(new TaskQueue);
            threads_.PushBack(std::unique_ptr<std::thread>(
                new std::thread([this, core]{
                    std::shared_ptr<Processor> processor(
                            new Processor(
                                num_workers_per_core_,
                                *task_queues_[core], stop_)
                    );
                    processors_.EmplaceBack(processor);
                    processor->Run();
                }))
            );
        }
    }
    ~ProcessorPool() {
        Finalize();
    }
    void Finalize() {
        stop_.store(true, std::memory_order_release);
        for (const auto& thread : threads_) {
            thread->join();
        }
    }
    void AddTask(const Task& task) {
        // add task in a round-robin manner
        task_lock_.lock();
        last_core_ = (last_core_ + 1) % num_cores_;
        const auto last_core = last_core_;
        task_lock_.unlock();
        task_queues_[last_core]->Push(task);
    }
private:
    uint64_t num_cores_;
    uint64_t num_workers_per_core_;
    uint64_t last_core_;
    SpinLock task_lock_;
    std::atomic<bool> stop_;
    SmallVector<std::unique_ptr<TaskQueue>> task_queues_;
    SmallVector<std::unique_ptr<std::thread>> threads_;
    SmallVector<std::shared_ptr<Processor>> processors_;
};
}  // namespace coro

