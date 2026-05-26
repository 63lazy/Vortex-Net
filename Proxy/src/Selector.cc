#include "Selector.h"
#include <Vortex-Net/EventLoop.h>
#include <Vortex-Net/logger.h>
std::atomic_int Selector::total_weight=0;
std::atomic_int Selector::failed_requests=0;
void Selector::addNode(const std::shared_ptr<ServerNode> node){
    //待改进：写时复制而非加锁
    {
        std::unique_lock<std::mutex> lock(mutex_);
        serverGroup_.emplace_back(node);
    }
}   

InetAddress Selector::getNextServer(){
    //待改进：写时复制而非加锁
    std::unique_lock<std::mutex> lock(mutex_);
    //集群组中没有服务器
    if(serverGroup_.empty()) 
    {   
        LOG_ERROR("No available backend servers!");
        failed_requests++;
        return InetAddress(0,"0.0.0.0");
    }
    //统计活着的节点比例
    int alive_count = 0;
    for(const auto& node : serverGroup_) {
        if(node->isAlive()) alive_count++;
    }
    //判定是否触发 Panic Mode
    //如果活着的节点太少，说明 HealthChecker 可能失真，强制“全量复活”
    bool panic_mode = (alive_count * 100 / serverGroup_.size()) < 20;

    ServerNode* best = nullptr;
    int total = 0;
    for (auto& node : serverGroup_) {
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