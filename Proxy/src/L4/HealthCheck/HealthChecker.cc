#include "HealthChecker.h"
#include <Vortex-Net/logger.h>
void HealthChecker::addNode(const std::shared_ptr<ServerNode> node){
    std::string name="CheckerClient";
    tasks_.emplace_back(CheckerTask::create(loop_, name, node));
}   

void HealthChecker::start(){
    if(tasks_.empty()) {
        LOG_INFO("No available node");
        return ;
    }
    //计算打散事件
    double pacing = static_cast<double>(checkInterval_) / tasks_.size();

    //待 增删节点后需要停止后手动修改间隔时间
    loop_->runEvery(pacing,[this](){
        tasks_[TaskIndex_]->start();
        TaskIndex_=(TaskIndex_+1)% tasks_.size();
    });
}