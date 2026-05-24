#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include <memory>
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseloop, const std::string &name)
    :baseloop_(baseloop)
    ,name_(name)
    ,started_(false)
    ,numThreads_(0)
    ,next_(0)
{
}
void EventLoopThreadPool::start(const ThreadInitCallback &cb){
    started_=true;
    for(int i=0;i<numThreads_;i++){
        char name_buf[name_.size()+32];
        snprintf(name_buf,sizeof(name_buf),"%s-%d",name_.c_str(),i);
        EventLoopThread *t= new EventLoopThread(cb,name_buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        //startLoop创建并返回一个EventLoop的地址//
        loops_.push_back(t->startLoop());
    }
    //整个服务端只有一个线程，运行这baseloop_
    if(numThreads_==0&&cb){
        cb(baseloop_);
    }
}
//默认轮询分配
EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop *loop=baseloop_;

    if(!loops_.empty())
    {
        loop=loops_[next_];
        ++next_;
        if(next_>=static_cast<ssize_t>(loops_.size())){next_=0;}
    }
    return loop;
}
std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty())
    {
        return std::vector<EventLoop*>(1,baseloop_);
    }
    else{
        return loops_;
    }
}