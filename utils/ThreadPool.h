#pragma once
#include "thread.h"
#include <vector>
#include <queue>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
class ThreadPool{
public:
    ThreadPool(int nums);
    ~ThreadPool();
    template<typename F,typename ...Args>
    void enqueue(F &&f,Args &&...Args);

private:
    std::vector<Thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::atomic<bool> stop_;
    std::mutex mtx_;
    std::condition_variable cv_;
};