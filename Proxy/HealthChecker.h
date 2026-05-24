#pragma once
#include "CheckerTask.h"
#include <vector>
#include <atomic>
#include <mymuduo/EventLoop.h>
class HealthChecker{
public:
    using task_ptr=std::shared_ptr<CheckerTask>;
    HealthChecker(EventLoop *loop):loop_(loop){};
    void addNode(const std::shared_ptr<ServerNode> node);
    void start();

private:

    EventLoop *loop_;

    //五秒进行一轮
    int checkInterval_{5};
    std::vector<task_ptr> tasks_;

    std::atomic_int TaskIndex_{0};
};