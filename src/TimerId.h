#pragma once
#include "Timer.h"
class TimerId{
public:
    TimerId(Timer* timer,uint64_t seq):
            timer_(timer),
            sequence_(seq)
    {}

    friend class TimerQueue;
    
private:
    Timer *timer_;
    uint64_t sequence_;
};