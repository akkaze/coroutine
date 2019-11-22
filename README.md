# coroutine  
A cross platform header only coroutine and coroutine pool library  
### Build  
You can just copy files to your project, but there is still makefile if you need to test it on unix-like system  
Just run ***make*** in terminal, then you can excutable named exmaple, run it  directly  
### Tutorial  
A very simple c++ example is presented.  
```cpp
#include <thread>
#include <mutex>
#include <iostream>
#include "coroutine.h"
#include "processor_pool.h"

static std::mutex log_lock;
void Print(int  k) {
    for (int i = 0; i < 3; i++) {
        log_lock.lock();
        std::cout << "thread id : " << std::this_thread::get_id()
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
```  
output to terminal may looks like these  
```python
thread id: 140325076911872	3
thread id: 140325076911872	3
thread id: 140325068519168	0
thread id: 140325068519168	0
thread id: 140325068519168	4
thread id: 140325068519168	0
thread id: 140325068519168	8
thread id: 140325068519168	4
thread id: 140325068519168	8
thread id: 140325068519168	4
thread id: 140325068519168	8
thread id: 140325051733760	2
thread id: 140325060126464	1
thread id: 140325060126464	1
thread id: 140325060126464	5
thread id: 140325060126464	1
thread id: 140325076911872	7
thread id: 140325060126464	9
thread id: 140325060126464	5
thread id: 140325060126464	9
thread id: 140325060126464	5
thread id: 140325076911872	3
thread id: 140325076911872	7
thread id: 140325076911872	7
thread id: 140325060126464	9
thread id: 140325051733760	2
thread id: 140325051733760	6
thread id: 140325051733760	2
thread id: 140325051733760	6
thread id: 140325051733760	6
```
