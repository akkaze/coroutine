#pragma once

#ifndef STACK_LIMIT
#define STACK_LIMIT (1024*1024)
#endif

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cassert>

#include <string>
#include <vector>
#include <list>
#include <thread>
#include <future>

using ::std::string;
using ::std::wstring;

#ifdef _MSC_VER
#include <Windows.h>
#else
#if defined(__APPLE__) && defined(__MACH__)
#define _XOPEN_SOURCE
#include <ucontext.h>
#else
#include <ucontext.h>
#endif
#endif

namespace coro {

typedef unsigned routine_t;

#ifdef _MSC_VER

struct Routine {
    std::function<void()> func;
    bool finished;
    LPVOID fiber;

    Routine(std::function<void()> f) {
        func = f;
        finished = false;
        fiber = nullptr;
    }

    ~Routine() {
        DeleteFiber(fiber);
    }
};

struct Ordinator {
    std::vector<Routine *> routines;
    std::list<routine_t> indexes;
    routine_t current;
    size_t stack_size;
    LPVOID fiber;

    Ordinator(size_t ss = STACK_LIMIT) {
        current = 0;
        stack_size = ss;
        fiber = ConvertThreadToFiber(nullptr);
    }

    ~Ordinator() {
        for (auto &routine : routines)
            delete routine;
    }
};

thread_local static Ordinator ordinator;

inline routine_t Create(std::function<void()> f) {
    Routine *routine = new Routine(f);

    if (ordinator.indexes.empty()) {
        ordinator.routines.push_back(routine);
        return ordinator.routines.size();
    }
    else {
        routine_t id = ordinator.indexes.front();
        ordinator.indexes.pop_front();
        assert(ordinator.routines[id - 1] == nullptr);
        ordinator.routines[id - 1] = routine;
        return id;
    }
}

inline void Destroy(routine_t id) {
    Routine *routine = ordinator.routines[id - 1];
    assert(routine != nullptr);

    delete routine;
    ordinator.routines[id - 1] = nullptr;
    ordinator.indexes.push_back(id);
}

inline void __stdcall Entry(LPVOID lpParameter = nullptr) {
    routine_t id = ordinator.current;
    Routine *routine = ordinator.routines[id - 1];
    assert(routine != nullptr);

    routine->func();

    routine->finished = true;
    ordinator.current = 0;

    SwitchToFiber(ordinator.fiber);
}

inline int Resume(routine_t id) {
    assert(ordinator.current == 0);
    Routine *routine = ordinator.routines[id - 1];
    if (routine == nullptr)
        return -1;

    if (routine->finished)
        return -2;

    if (routine->fiber == nullptr) {
        routine->fiber = CreateFiber(ordinator.stack_size, entry, 0);
        ordinator.current = id;
        SwitchToFiber(routine->fiber);
    }
    else {
        ordinator.current = id;
        SwitchToFiber(routine->fiber);
    }

    return 0;
}

inline void Yield() {
    routine_t id = ordinator.current;
    Routine *routine = ordinator.routines[id - 1];
    assert(routine != nullptr);

    ordinator.current = 0;
    SwitchToFiber(ordinator.fiber);
}

inline routine_t Current() {
    return ordinator.current;
}

#if 0
template<typename Function>
inline typename std::result_of<Function()>::type
Await(Function &&func)
{
    auto future = std::async(std::launch::async, func);
    std::future_status status = future.wait_for(std::chrono::milliseconds(0));

    while (status == std::future_status::timeout) {
        if (ordinator.current != 0)
            Yield();

        status = future.wait_for(std::chrono::milliseconds(0));
    }
    return future.get();
}
#endif

#if 1
template<typename Function>
inline std::result_of_t<std::decay_t<Function>()>
Await(Function &&func) {
    auto future = std::async(std::launch::async, func);
    std::future_status status = future.wait_for(std::chrono::milliseconds(0));

    while (status == std::future_status::timeout)
    {
        if (ordinator.current != 0)
            Yield();

        status = future.wait_for(std::chrono::milliseconds(0));
    }
    return future.get();
}
#endif

#else    // unix

struct Routine {
    std::function<void()> func;
    char *stack;
    bool finished;
    ucontext_t ctx;

    Routine(std::function<void()> f) {
        func = f;
        stack = nullptr;
        finished = false;
    }

    ~Routine() {
        delete[] stack;
    }
};

struct Ordinator {
    std::vector<Routine *> routines;
    std::list<routine_t> indexes;
    routine_t current;
    size_t stack_size;
    ucontext_t ctx;

    inline Ordinator(size_t ss = STACK_LIMIT) {
        current = 0;
        stack_size = ss;
    }

    inline ~Ordinator() {
        for (auto &routine : routines)
            delete routine;
    }
};

thread_local static Ordinator ordinator;

inline routine_t Create(std::function<void()> f) {
    Routine *routine = new Routine(f);

    if (ordinator.indexes.empty()) {
        ordinator.routines.push_back(routine);
        return ordinator.routines.size();
    }
    else {
        routine_t id = ordinator.indexes.front();
        ordinator.indexes.pop_front();
        assert(ordinator.routines[id - 1] == nullptr);
        ordinator.routines[id - 1] = routine;
        return id;
    }
}

inline void Destroy(routine_t id) {
    Routine *routine = ordinator.routines[id - 1];
    assert(routine != nullptr);

    delete routine;
    ordinator.routines[id - 1] = nullptr;
}

inline void Entry() {
    routine_t id = ordinator.current;
    Routine *routine = ordinator.routines[id - 1];
    routine->func();

    routine->finished = true;
    ordinator.current = 0;
    ordinator.indexes.push_back(id);
}

inline int Resume(routine_t id) {
    assert(ordinator.current == 0);

    Routine* routine = ordinator.routines[id - 1];
    if (routine == nullptr)
        return -1;

    if (routine->finished)
        return -2;

    if (routine->stack == nullptr) {
        //initializes the structure to the currently active context.
        //When successful, getcontext() returns 0
        //On error, return -1 and set errno appropriately.
        getcontext(&routine->ctx);

        //Before invoking makecontext(), the caller must allocate a new stack
        //for this context and assign its address to ucp->uc_stack,
        //and define a successor context and assign its address to ucp->uc_link.
        routine->stack = new char[ordinator.stack_size];
        routine->ctx.uc_stack.ss_sp = routine->stack;
        routine->ctx.uc_stack.ss_size = ordinator.stack_size;
        routine->ctx.uc_link = &ordinator.ctx;
        ordinator.current = id;

        //When this context is later activated by swapcontext(), the function entry is called.
        //When this function returns, the  successor context is activated.
        //If the successor context pointer is NULL, the thread exits.
        makecontext(&routine->ctx, reinterpret_cast<void (*)(void)>(Entry), 0);

        //The swapcontext() function saves the current context,
        //and then activates the context of another.
        swapcontext(&ordinator.ctx, &routine->ctx);
    }
    else {
        ordinator.current = id;
        swapcontext(&ordinator.ctx, &routine->ctx);
    }

    return 0;
}

inline void Yield() {
    routine_t id = ordinator.current;
    Routine *routine = ordinator.routines[id - 1];
    assert(routine != nullptr);

    char *stack_top = routine->stack + ordinator.stack_size;
    char stack_bottom = 0;
    assert(size_t(stack_top - &stack_bottom) <= ordinator.stack_size);

    ordinator.current = 0;
    swapcontext(&routine->ctx , &ordinator.ctx);
}

inline routine_t Current() {
    return ordinator.current;
}

template<typename Function>
inline typename std::result_of<Function()>::type
Await(Function &&func) {
    auto future = std::async(std::launch::async, func);
    std::future_status status = future.wait_for(std::chrono::milliseconds(0));

    while (status == std::future_status::timeout) {
        if (ordinator.current != 0)
            Yield();

        status = future.wait_for(std::chrono::milliseconds(0));
    }
    return future.get();
}

#endif

template<typename Type>
class Channel {
public:
    Channel() {
        taker_ = 0;
    }

    Channel(routine_t id) {
        taker_ = id;
    }

    inline void Consumer(routine_t id) {
        taker_ = id;
    }

    inline void Push(const Type &obj) {
        list_.push_back(obj);
        if (taker_ && taker_ != Current())
            Resume(taker_);
    }

    inline void Push(Type &&obj) {
        list_.push_back(std::move(obj));
        if (taker_ && taker_ != Current())
            Resume(taker_);
    }

    inline bool Pop(Type& obj) {
        if (!taker_)
            taker_ = Current();
        while (list_.empty() && !closed_.load(std::memory_order_release))
            Yield();
        if (list_.empty() && closed_.load(std::memory_order_release)) {
            return false;
        }
        obj = std::move(list_.front());
        list_.pop_front();
        return true;
    }
    inline void Close() {
        closed_.store(true, std::memory_order_release);
    }
    inline void Clear() {
        list_.clear();
    }

    inline void Touch() {
        if (taker_ && taker_ != Current())
            Resume(taker_);
    }

    inline size_t Size() {
        return list_.size();
    }

    inline bool IsEmpty() {
        return list_.empty();
    }

private:
    std::list<Type> list_;
    routine_t taker_;
    std::atomic<bool> closed_;
};

}
