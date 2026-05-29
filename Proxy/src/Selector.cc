#include "Selector.h"
#include <Vortex-Net/EventLoop.h>
#include <Vortex-Net/logger.h>

std::atomic_int Selector::total_weight=0;
std::atomic_int Selector::failed_requests=0;
void Selector::addNode(const std::shared_ptr<ServerNode> node){
    //修改节点必须加锁防止重入
    std::unique_lock<std::mutex> lock(mutex_);
    //说明此时有线程在读
    if(serverGroup_.use_count() > 1){
        //写时复制逻辑
        //写操作直接操作新副本
        serverGroup_.reset(new ServerNodeList(*serverGroup_));
    }
    serverGroup_->emplace_back(node);

    if(ring_.use_count() > 1){
        ring_.reset(new std::map<size_t, InetAddress>(*ring_));
    }
    std::string ip_port=node->getAddr().toIpPort();
    //快速挂载一致性哈希虚拟节点
    for(int i=0;i<kVirtualNodes;i++){
        std::string vnode_name = ip_port + "#" + std::to_string(i);
        size_t partition=std::hash<std::string>{}(vnode_name);
       (*ring_)[partition]=node->getAddr();
    }
}   
void Selector::removeNode(const std::shared_ptr<ServerNode> node){
    std::unique_lock<std::mutex> lock(mutex_);
    if(serverGroup_.use_count() > 1){
        serverGroup_.reset(new ServerNodeList(*serverGroup_));
    }
    auto &nodes=*serverGroup_;
    nodes.erase(std::remove(nodes.begin(),nodes.end(),node),nodes.end());


    if(ring_.use_count() > 1){
        ring_.reset(new std::map<size_t, InetAddress>(*ring_));
    }
    std::string ip_port=node->getAddr().toIpPort();
    for(int i=0;i<kVirtualNodes;i++){
        std::string vnode_name = ip_port + "#" + std::to_string(i);
        size_t partition=std::hash<std::string>{}(vnode_name);
       (*ring_).erase(partition);
    }
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
//一致性哈希算法
InetAddress Selector::getNextServer(const std::string& key){
    std::shared_ptr<std::map<size_t, InetAddress>> ring;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ring=ring_;
    }
    if(ring->empty()){
        return InetAddress(0,"0.0.0.0");
    }
    size_t hash_val = std::hash<std::string>{}(key);

    auto it=ring->lower_bound(hash_val);
    if(it==ring->end()){
        it=ring->begin();
    }
    return it->second;
}