#include <thread>
#include <mutex>
#include <iostream>
#include "coroutine.h"
#include "processor_pool.h"

static std::mutex log_lock;
void Print(int  k) {
    for (int i = 0; i < 3; i++) {
        log_lock.lock();
        std::cout << "thread id: " << std::this_thread::get_id()
            <<  "\t" << k << '\n';
        log_lock.unlock();
        // remember yield may be useful when you want to use pool, but is not compulisive,
        // it can imporve concurency in many cases
        coro::Yield();
    }
}
void Produce(coro::ProcessorPool& pool) {
    for (int i = 0; i < 10; i++) {
        pool.AddTask(std::bind(Print, i));
    }
}

int main() {
    // the first argument is number of threads you want to use, it can be ignored, the default value
    // will be number of cores of your machine
    // the second argument is number of coroutines resident in each of the threads, feel free to increase
    // the number, it will not occupy much cpu resource
    coro::ProcessorPool pool(4, 4);
    Produce(pool);
    return 0;
}
