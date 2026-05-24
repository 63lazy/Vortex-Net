#pragma once
#include <functional>
#include "noncopy.h"
#include "Timestamp.h"
#include "CurrentThread.h"
#include "TimerQueue.h"
#include "TimerId.h"
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
class Poller;
class Channel;

class EventLoop:NonCopyable{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();
    void loop(); //开启事件循环
    void quit(); //退出事件循环

    Timestamp returnTime(){return pollReturnTime_;}

    void runInLoop(Functor cb); //在当前loop中执行cb回调操作
    void queueInLoop(Functor cb); //将cb回调操作加入到队列中
        
    //在指定的绝对时间点执行
    TimerId runAt(Timestamp time,TimerCallback cb){
        return timerQueue_->addTimer(std::move(cb), time, 0.0);
    }
    //在经过指定的延迟秒数后执行
    TimerId runAfter(double delay,TimerCallback cb){
        Timestamp time(addTime(Timestamp::now(),delay));
        return runAt(time,std::move(cb));
    }
    //每隔指定的秒数重复执行
    TimerId runEvery(double interval,TimerCallback cb){
        //设置第一次触发时间为now+interval
        Timestamp time(addTime(Timestamp::now(), interval));
        // 传入 interval，TimerQueue内部handleRead会处理为 repeat逻辑
        return timerQueue_->addTimer(std::move(cb),time,interval);
    }
    void cancel(TimerId timerid){
        return timerQueue_->cancel(timerid);
    }
    
    void wakeup();

    void updateChannel(Channel* channel); //更新channel
    void removeChannel(Channel* channel); //移除channel
    bool hasChannel(Channel* channel); //判断channel是否在当前loop中

    bool isInLoopThread() const{return threadId_==CurrentThread::tid();}

private:
    void handleReading();//用于wakeup
    void doPendingFunctors();//执行pendingFunctors_中的回调操作


    using ChannelList = std::vector<Channel*>;

    std::atomic<bool> looping_;
    std::atomic<bool> quit_;     //标志退出loop循环

    const pid_t threadId_; //记录当前loop所在线程的id
    Timestamp pollReturnTime_; //poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_; //用于唤醒subloop所在线程的fd(轮询的方式)
    std::unique_ptr<Channel> wakeupChannel_; //用于处理wakeupFd_的channel

    ChannelList activeChannels_; //poller返回的发生事件的channels
    
    std::atomic<bool> callingPendingFunctors_; //标志是否正在执行回调操作
    std::vector<Functor> pendingFunctors_; //存储loop需要的所有回调操作 跨线程安全的唯一通道
    std::mutex mutex_; //保护pendingFunctors_的互斥锁
};