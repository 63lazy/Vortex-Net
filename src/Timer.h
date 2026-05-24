#pragma once
#include "noncopy.h"
#include "Timestamp.h"
#include  "Callbacks.h"
class Timer : NonCopyable{
public:
    Timer(TimerCallback cb,Timestamp when,double interval);
    void run() const{
        callback_();
    }

    //重新设置计时器过期时间
    void restart(Timestamp now);
    bool repeat() const {return repeat_;}
    uint64_t sequence() const {return sequence_;}
    Timestamp expiration() const {return expiration_;}
private:
    const TimerCallback callback_;
    //记录绝对过期时间 即到达expiration_这个时间点就响铃
    Timestamp expiration_;
    //每次倒计时间隔时长
    const double interval_;
    //是否是重复任务
    const bool repeat_;
    const int64_t sequence_;
    static std::atomic<int64_t> num_created;
};