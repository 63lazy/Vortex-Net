// ServerNode.h
#pragma once
#include <Vortex-Net/InetAddress.h>
#include <Vortex-Net/logger.h>
#include <atomic>

class ServerNode {
public:
    ServerNode(const InetAddress& addr, int weight, int s_std = 3, int f_std = 2)
        : addr_(addr), weight_(weight), 
          success_standard_(s_std), fail_standard_(f_std),
          is_alive_(true) {}

    // 修改节点状态
    void updateStatus(bool success)
    {
        if(success){
            fail_count_=0;
            success_count_++;
            if(success_count_>=success_standard_){
                is_alive_=true;
                LOG_INFO("node:%s %s lived",getAddr().toIp().c_str(),getAddr().toIpPort().c_str());
            }
        }
        else{
            success_count_=0;
            fail_count_++;
            if(fail_count_>=fail_standard_){
                is_alive_=false;
                LOG_INFO("node:%s %s died",getAddr().toIp().c_str(),getAddr().toIpPort().c_str());
            }
        }
    }
    
    bool isAlive() const { return is_alive_.load(); }
    int getWeight() const { return weight_; }
    InetAddress getAddr() const { return addr_; }

    // 未来在这里增加“慢启动”逻辑
    int getEffectiveWeight() const;
    //待 SWRR 算法用的临时变量（注意：这个变量由 Selector 线程独占，不需要 atomic）
    int current_weight_{0}; 
private:
    InetAddress addr_;
    int weight_;
    
    int success_standard_;
    int fail_standard_;

    std::atomic<bool> is_alive_;
    std::atomic<int> success_count_{0};
    std::atomic<int> fail_count_{0};

};