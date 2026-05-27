#include "Selector.h"
#include <Vortex-Net/EventLoop.h>
#include <Vortex-Net/logger.h>
std::atomic_int Selector::total_weight=0;
std::atomic_int Selector::failed_requests=0;
void Selector::addNode(const std::shared_ptr<ServerNode> node){
    //修改节点必须加锁防止重入
    std::unique_lock<std::mutex> lock(mutex_);
    //说明此时有线程在读
    if(!serverGroup_.unique()){
        //写时复制逻辑
        //写操作直接操作新副本
        serverGroup_.reset(new ServerNodeList(*serverGroup_));
    }
    serverGroup_->emplace_back(node);
}   

InetAddress Selector::getNextServer(){
    //高频操作 如果用加锁的方式会导致性能下降
    //采取写时复制
    std::shared_ptr<ServerNodeList> nodes;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        nodes=serverGroup_;
    }

    //集群组中没有服务器
    if((*nodes).empty()) 
    {
        LOG_ERROR("No available backend servers!");
        failed_requests++;
        return InetAddress(0,"0.0.0.0");
    }
    //统计活着的节点比例
    int alive_count = 0;
    for(const auto& node : (*nodes)) {
        if(node->isAlive()) alive_count++;
    }
    //判定是否触发 Panic Mode
    //如果活着的节点太少，说明 HealthChecker 可能失真，强制“全量复活”
    bool panic_mode = (alive_count * 100 / (*nodes).size()) < 20;

    ServerNode* best = nullptr;
    int total = 0;
    for (auto& node : (*nodes)) {
        if (!node->isAlive()&&!panic_mode) continue;
        
        node->current_weight_ += node->getWeight();
        total += node->getWeight();

        if (!best || node->current_weight_ > best->current_weight_) {
            best = &(*node);
        }
    }
    if (best) {
        best->current_weight_ -= total;
        return best->getAddr();
    }
    //服务器全都挂掉了
    else{
        LOG_ERROR("No alived backend servers!");
        return InetAddress(0,"0.0.0.0");
    }
}