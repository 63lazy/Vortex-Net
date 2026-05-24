#include "ThreadPool.h"
#include "thread.h"

ThreadPool::ThreadPool(int nums):stop_(false)
{
    for(int i=0;i<nums;i++){
        threads_.emplace_back([this](){
            while(1){
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(mtx_);
                    cv_.wait(lock,[this](){
                        return !tasks_.empty()||stop_;
                    });
                    if(stop_){
                        return ;
                    }
                    task=std::move(tasks_.front());
                    tasks_.pop();
                }
                
                task();
            }
        });
    
    }

}
ThreadPool::~ThreadPool(){
    stop_=true;
    cv_.notify_all();
    for(auto &thread : threads_){
        thread.join();
    }
}
template<typename F,typename ...Args>
void ThreadPool::enqueue(F &&f,Args &&...args){
    auto task=[Func=forward<F>(f),func_args=forward<Args>(args)]() mutable{
        Func(func_args);
    };
    {
        std::unique_lock<std::mutex> lock(mtx);
        tasks_.emplace(move(task));
        cv.notify_one();
    }
}
