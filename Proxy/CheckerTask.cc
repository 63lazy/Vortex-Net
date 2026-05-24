#include "CheckerTask.h"
#include "ServerNode.h"
#include <netinet/tcp.h> 
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
    client_->setErrorCallback([this](int saveError){
        onError(saveError);
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

        //强制发送 RST
        struct linger sl;
        sl.l_onoff = 1;  // 开启 Linger
        sl.l_linger = 0; // 逗留时间为 0
        int fd = conn->fd(); 
        if (::setsockopt(fd, SOL_SOCKET, SO_LINGER, &sl, sizeof(sl)) < 0) {
            LOG_ERROR("setsockopt SO_LINGER failed, errno: %d", errno);
        }
        //forceClose()->handleClose() -> channel_->remove() -> close()->发送RST包
        conn->forceClose(); 
        is_probing_=false;
    }
    else{
        is_probing_=false;
    }
}
void CheckerTask::onError(int saveError){
    if (has_determined_.exchange(true)) {
        return;
    }
    //LB自己的临时端口耗尽或内核缓冲区拥塞 ||fd达到上限
    if(saveError==EAGAIN|| saveError == EMFILE){
        //允许探测直接结束
        is_probing_=false;
        return;
        LOG_WARN("Local resource limit reached, skipping probe for %s",node_->getAddr().toIpPort().c_str());
    }
    LOG_ERROR("Probe failed for %s Error: %d",node_->getAddr().toIpPort().c_str(),strerror(saveError));

    loop_->cancel(timerId_);
    node_->updateStatus(true);

    client_.stop();
}