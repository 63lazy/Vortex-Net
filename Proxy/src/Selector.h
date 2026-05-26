#pragma once
#include <vector>
#include <Vortex-Net/Callbacks.h>
#include <Vortex-Net/InetAddress.h>
#include <Vortex-Net/EventLoop.h>
#include <mutex>
#include <atomic>
#include <memory>
#include "ServerNode.h"
class Selector{
public:
    Selector(EventLoop *loop):loop_(loop){}
    void addNode(const std::shared_ptr<ServerNode> node);
    InetAddress getNextServer();
private:
    std::vector<std::shared_ptr<ServerNode>> serverGroup_;
    EventLoop *loop_;
    
    std::mutex mutex_;
    static std::atomic_int total_weight;
    static std::atomic_int failed_requests;

};