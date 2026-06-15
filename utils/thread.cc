#include "thread.h"
#include "CurrentThread.h"
#include <semaphore.h>
std::atomic<int> Thread::numCreated_=0;
Thread::Thread(ThreadFunc func,const std::string& name)
    :started_(false),
    joined_(false),
    tid_(0),
    func_(std::move(func)),
    name_(name)
{
    setDefaultName();
}
//待 工业级网络库通常更倾向于在析构时，通过原子变量或信号通知线程停止，并调用 join() 等待其真正退出，以确保资源完全回收。
Thread::~Thread(){
    //thread必须join或者detach二选一
    if(started_&&!joined_){
        thread_->detach();
    }
}
void Thread::start(){
    started_=true;
    auto sem=std::make_shared<sem_t>(); //分配堆上的内存 防止主线程结束过快 sem_t被快速回收
    sem_init(sem.get(),false,0);
    //待 最好用[self = shared_from_this()]
    thread_=std::make_shared<std::thread>([this,sem](){//捕获智能指针增加引用计数
        tid_=CurrentThread::tid();
        sem_post(sem.get());
        func_();
    });
    sem_wait(sem.get());
}
void Thread::join(){
    joined_=true;
    thread_->join();
}

void Thread::setDefaultName(){
    int num=++numCreated_;
    if(name_.empty()){
        name_="Thread-"+std::to_string(num);
    }
}