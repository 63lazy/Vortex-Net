#include "Timer.h"
#include "Timestamp.h"
#include <atomic>

std::atomic<int64_t> Timer::num_created=0;
void Timer::restart(Timestamp now){
    if(repeat_){
        expiration_=addTime(now,interval_);
    }
    else{
        expiration_=Timestamp::invalid();
    }
}
Timer::Timer(TimerCallback cb,Timestamp when,double interval):
        callback_(cb),
        expiration_(when),
        interval_(interval),
        repeat_(interval > 0),
        sequence_(++num_created)
{}