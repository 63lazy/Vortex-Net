#pragma once
#include "CheckerTask.h"
#include <vector>
#include <atomic>
#include <Vortex-Net/EventLoop.h>
class HealthChecker{
public:
    using task_ptr=std::shared_ptr<CheckerTask>;
    HealthChecker(EventLoop *loop):loop_(loop){};
    void addNode(const std::shared_ptr<ServerNode> node);
    void start();

private:

    EventLoop *loop_;
    //每轮的时间
    int checkInterval_{3};
    std::vector<task_ptr> tasks_;

    std::atomic_int TaskIndex_{0};
};