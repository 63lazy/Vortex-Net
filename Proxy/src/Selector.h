#pragma once
#include <Vortex-Net/Callbacks.h>
#include <Vortex-Net/InetAddress.h>
#include <Vortex-Net/EventLoop.h>
#include <map>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include "ServerNode.h"
class Selector{
public:
    using ServerNodeList=std::vector<std::shared_ptr<ServerNode>>;
    Selector(EventLoop *loop):loop_(loop), serverGroup_(std::make_shared<ServerNodeList>()), ring_(std::make_shared<std::map<size_t, InetAddress>>()) {}
    void addNode(const std::shared_ptr<ServerNode> node);
    void removeNode(const std::shared_ptr<ServerNode> node);
    InetAddress getNextServer();
    InetAddress getNextServer(const std::string& key);
private:
    //配合写时复制使用
    
    EventLoop *loop_;
    std::shared_ptr<ServerNodeList> serverGroup_;
    std::mutex mutex_;
    static std::atomic_int total_weight;
    static std::atomic_int failed_requests;

    //哈希环
    std::shared_ptr<std::map<size_t, InetAddress>> ring_;

    const int kVirtualNodes = 100;
};