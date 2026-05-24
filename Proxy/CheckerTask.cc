#include "CheckerTask.h"
#include "ServerNode.h"
CheckerTask::CheckerTask(EventLoop *loop,std::string &name,const std::shared_ptr<ServerNode> node):
                client_(std::make_shared<TcpClient>(loop,node->getAddr(),name)),
                loop_(loop),
                node_(node),
                has_determined_(false)
{
    //待 （捕获裸指针）
    client_->setConnectionCallback([this](const TcpConnectionPtr &conn){
        onConnection(conn);
    });
}

void CheckerTask::start(){
    if(is_probing_){
        return;
    }
    is_probing_=true;
    has_determined_=false;

    client_->connect();
    //待（野指针问题）
    timerId_=loop_->runAfter(2.0,[this](){
        onTimeout();
    });
}

void CheckerTask::onTimeout(){
    //has_determined_判断是否是连接成功后没来得及断开，先触发了onTimeout的情况
    if (has_determined_.exchange(true)) {
        return; //说明 onConnection 已经先出结果了，超时判定无效
    }
    LOG_INFO("node:%s %s connect failed",node_->getAddr().toIp().c_str(),node_->getAddr().toIpPort().c_str());
    node_->updateStatus(false);
    client_->stop();
}

void CheckerTask::onConnection(const TcpConnectionPtr &conn){
    //不管成败先停掉定时器
    if(timerId_)
        loop_->cancel(*timerId_);
    if(conn->connected()){
        if (has_determined_.exchange(true)) {
            return; //说明 onTimeout 先出结果了
        }
        has_determined_=true;
        node_->updateStatus(true);
        LOG_INFO("node:%s %s connect success",node_->getAddr().toIp().c_str(),node_->getAddr().toIpPort().c_str());
        conn->shutdown();
    }
    else{
        is_probing_=false;
    }
}