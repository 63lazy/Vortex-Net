#pragma once
#include "Channel.h"
#include "Timestamp.h"
#include "Timer.h"
#include "TimerId.h"
#include <set>
class EventLoop;
using Entry=std::pair<Timestamp,Timer*>;
class TimerQueue{
public:
    explicit TimerQueue(EventLoop *loop);

    ~TimerQueue();
    //添加定时任务
    TimerId addTimer(TimerCallback cb,
                  Timestamp when,
                  double interval);
    void addTimerInLoop(Timer*);
    //取消任务
    void cancel(TimerId timerid);
private:
    void handleRead();
    std::vector<Entry> getExpired(Timestamp now);
    bool insert(Timer* timer);
    EventLoop *loop_;
    const int timerfd_;
    Channel timerChannel_;
    std::set<Entry> timers_;
    
    void cancelInLoop(TimerId timerid);
    //防止在回调执行中产生重入问题
    bool callingExpiredTimers_;
    //可以执行的任务 用于cancel判断任务是否需要删除
    std::set<Timer*> activeTimers_;

    std::set<Timer*> cancelingTimers_;
};