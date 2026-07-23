#include "CheckerTask.h"
#include "Common/ServerNode.h"
#include <cstring> 
#include <netinet/tcp.h> 
std::shared_ptr<CheckerTask> CheckerTask::create(EventLoop *loop,
                                    std::string &name,
                                    const std::shared_ptr<ServerNode> node)
{
    auto task = std::shared_ptr<CheckerTask>(new CheckerTask(loop, name, node));
    task->Init();
    return task;
}

CheckerTask::CheckerTask(EventLoop *loop,std::string &name,const std::shared_ptr<ServerNode> node):
                client_(std::make_shared<TcpClient>(loop,node->getAddr(),name)),
                loop_(loop),
                node_(node),
                has_determined_(false),
                is_probing_(false)
{}
void CheckerTask::Init(){
    auto self = shared_from_this();
    client_->setConnectionCallback([self](const TcpConnectionPtr &conn){
        self->onConnection(conn);
    });
    client_->setErrorCallback([self](int saveError){
        self->onError(saveError);
    });
}

void CheckerTask::start(){
    if(is_probing_){
        return;
    }
    is_probing_=true;
    has_determined_=false;

    client_->connect();

    auto self = shared_from_this();
    timerId_=loop_->runAfter(2.0,[self](){
        self->onTimeout();
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

    is_probing_ = false; 
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
//快速返回
void CheckerTask::onError(int saveError){
    if (has_determined_.exchange(true)) {
        return;
    }
    //本地探测任务自己的临时端口耗尽或内核缓冲区拥塞 ||fd达到上限
    if(saveError==EAGAIN|| saveError == EMFILE){
        //允许探测直接结束
        LOG_WARN("Local resource limit reached, skipping probe for %s",node_->getAddr().toIpPort().c_str());
        is_probing_=false;
        return;
    }
    LOG_ERROR("Probe failed for %s Error: %s",node_->getAddr().toIpPort().c_str(),strerror(saveError));

    loop_->cancel(*timerId_);
    node_->updateStatus(false);

    client_->stop();
    is_probing_=false;
}