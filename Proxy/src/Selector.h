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
    using ServerNodeList=std::vector<std::shared_ptr<ServerNode>>;
    Selector(EventLoop *loop):loop_(loop){}
    void addNode(const std::shared_ptr<ServerNode> node);
    InetAddress getNextServer();
private:
    //配合写时复制使用
    std::shared_ptr<ServerNodeList> serverGroup_;
    EventLoop *loop_;
    
    std::mutex mutex_;
    static std::atomic_int total_weight;
    static std::atomic_int failed_requests;

};