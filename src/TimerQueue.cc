#include "TimerQueue.h"
#include "logger.h"
#include "Channel.h"
#include "Timer.h"
#include "TimerId.h"
#include "EventLoop.h"
#include <sys/timerfd.h>
#include <cstring>
//创建timerfd CLOCK_MONOTONIC表示使用相对时间而非校对系统标准绝对时间
static int createTimefd(){
    int fd=::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(fd<0){
        LOG_FATAL("Failed in timerfd_create");
    }
    return fd;
}

void resetTimerfd(int timerfd,Timestamp expiration){
    struct itimerspec newValue;
    std::memset(&newValue,0,sizeof(newValue));
    // 计算下一次响铃距离现在的相对时间
    int64_t microseconds = expiration.microSecondsSinceEpoch() 
                         - Timestamp::now().microSecondsSinceEpoch();
    //如果时间已经过了，至少给内核留 100 微秒，防止定时器失效         
    if(microseconds<100){
        microseconds=100;
    }

    //将微秒转化为timespec结构
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / 1000000);
    ts.tv_nsec = static_cast<long>((microseconds % 1000000) * 1000);
    //填充itimerspec
    newValue.it_value=ts;
    //调用系统调用，更新内核闹钟
    // 0 代表相对时间，如果用 TFD_TIMER_ABSTIME 则代表绝对时间
    if(::timerfd_settime(timerfd,0,&newValue,nullptr)<0){
        LOG_ERROR("timerfd_settime() error");
    }
}

TimerQueue::TimerQueue(EventLoop *loop):
                       loop_(loop),
                       timerfd_(createTimefd()),
                       timerChannel_(loop,timerfd_),
                       timers_(),
                       callingExpiredTimers_(false)
{
    timerChannel_.setReadCallback([this](Timestamp recievetime){
        handleRead();
    });
    timerChannel_.enableReading();
}
void TimerQueue::handleRead(){
    Timestamp now(Timestamp::now());
    //nums用来记录有几个计时器到时间了
    uint64_t nums;
    ssize_t n=::read(timerfd_,&nums,sizeof(nums));
    if(n!=sizeof(nums)){
        LOG_ERROR("TimerQueue::handleRead() reads %ld bytes instead of 8",n);
    }
    //获取已超时的计时器
    std::vector<Entry> expired=move(getExpired(now));
    callingExpiredTimers_=true;
    //执行这些计时器的回调
    for(const auto &it : expired){
        it.second->run();
    }
    for(auto &it:expired){
        //如果该定时器需要重复且没有要求延迟删除则重启定时器则重新insert进入红黑树
        if(it.second->repeat()&&
           cancelingTimers_.find(it.second)==cancelingTimers_.end()){
            it.second->restart(now);
            insert(it.second);
        }
        else{
            delete it.second;
        }
    }
    cancelingTimers_.clear();
    
    //获取红黑树中最早的时间，重设内核闹钟
    if(!timers_.empty()){
        Timestamp nextExpired = timers_.begin()->second->expiration();
        resetTimerfd(timerfd_,nextExpired);
    }
}
TimerQueue::~TimerQueue(){
    for(auto it : timers_){
        delete it.second;
    }
}
//获取已超时的计时器
std::vector<Entry> TimerQueue::getExpired(Timestamp now){
    std::vector<Entry> expired;
    //UINTPTR_MAX是最大地址
    Entry sentry=std::pair(now,reinterpret_cast<Timer*>(UINTPTR_MAX));
    auto it=timers_.lower_bound(sentry);

    std::copy(timers_.begin(),it,std::back_inserter(expired));
    timers_.erase(timers_.begin(),it);
    for(auto &it:expired){
        activeTimers_.insert(it.second);
    }
    return expired;
}

TimerId TimerQueue::addTimer(TimerCallback cb,
                          Timestamp when,
                          double interval){
    Timer *timer = new Timer(std::move(cb),when,interval);
    loop_->runInLoop([this,timer](){
        addTimerInLoop(timer);
    });

    return TimerId(timer,timer->sequence());
}
void TimerQueue::addTimerInLoop(Timer *timer){
    bool erliest=insert(timer);

    if(erliest){
        resetTimerfd(timerfd_,timer->expiration());
    }
}
bool TimerQueue::insert(Timer* timer){
    //记录新Timer是否是最早的
    bool erliest=false;
    Timestamp when=timer->expiration();
    auto it=timers_.begin();
    //如果set为空或者新任务的响铃时间更早
    if(it==timers_.end()||when<it->first){
        erliest=true;
    }

    //插入红黑树
    timers_.insert(Entry(when,timer));
    activeTimers_.insert(timer);
    return erliest;
}

void TimerQueue::cancel(TimerId timerid){
    loop_->runInLoop([this,timerid](){
        cancelInLoop(timerid);
    });
}
void TimerQueue::cancelInLoop(TimerId timerid){
    Timer *timer=timerid.timer_;
    auto it=activeTimers_.find(timer);

    //说明timer还未执行
    if(it!=activeTimers_.end()){
        auto it=timers_.find({timer->expiration(),timer});
        //是第一个tiemr
        if(it==timers_.begin()){
            resetTimerfd(timerfd_,(*(it++)).first);
        }
        timers_.erase(Entry(timer->expiration(),timer));
        activeTimers_.erase(timer);
        delete timer;
    }
    else if(callingExpiredTimers_){
        cancelingTimers_.insert(timer);
    }
}