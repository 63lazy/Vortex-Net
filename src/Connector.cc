#include "Connector.h"
#include "logger.h"
#include "socket.h"
#include <stdlib.h>
#include <string>
const int Connector::kInitRetryDelayMs;
const int Connector::kMaxRetryDelayMs;
static int createNonblocking()
{
    int sockfd = ::socket(AF_INET ,SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,0);
    if(sockfd<0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d \n",__FILE__,__FUNCTION__,__LINE__,errno);
    }
    return sockfd;
}

static EventLoop *CheckloopNotNull(EventLoop *loop){
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null!",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}
Connector::Connector(EventLoop *loop,const InetAddress &serverAddr):
                     addr_(serverAddr),
                     loop_(CheckloopNotNull(loop)),                     
                     state_(kDisconnected),
                     connect_(false),
                     retryDelayMs_(500),
                     retry_(false)
{
    LOG_INFO("Connector::ctor[%p]",this);
}

void Connector::start(){
    connect_=true;
    loop_->runInLoop([this](){
        startInLoop();
    });
}
void Connector::startInLoop(){
    if(connect_) connect();
    else LOG_INFO ("Connector::Connector:do not connect");
}
void Connector::connect(){
    int sockfd=createNonblocking();
    int rev = Socket::connect(sockfd,&addr_);
    int savedErrno = (rev==0)? 0: errno;

    //发起连接时的状态
    switch(savedErrno)
    {
        case 0:
        case EINPROGRESS:  // 正在连接
        case EINTR:        // 被信号打断
        case EISCONN:      // 已经连上了
            connecting(sockfd);
            break;
        case EAGAIN:
        case ECONNREFUSED: // 拒绝连接
        case ENETUNREACH:  // 网络不可达
            if(retry_)
                retry(sockfd);
            break;
        default:
            ::close(sockfd);
            break;
    }
}

void Connector::connecting(int fd){
    setState(kConnecting);
    channel_.reset(new Channel(loop_,fd));
    channel_->setWriteCallback([this](){
        handleWrite();
    });
    channel_->setErrorCallback([this](){
        handleError();
    });
    
    channel_->enableWriting();
}

void Connector::handleWrite(){
    int err;
    socklen_t len=sizeof(err);
    int ret=::getsockopt(channel_->fd(),SOL_SOCKET, SO_ERROR, &err, &len);
    if(ret<0) err=errno;
    //连接成功了//
    if(err==0){
        setState(kConnected);
        if(connect_){
            int sockfd=channel_->fd();
            channel_->disableAll();
            channel_->remove();
            //用queueInLoop重置智能指针，确定loop中的handleEvent已经跑完后再删除
            auto self=shared_from_this();
            loop_->queueInLoop([self](){
                self->channel_.reset();
            });
            if(newConnectionCallback_)
            {
                newConnectionCallback_(sockfd);
            }
        }
        //可能连上了，但是用户中途调用了stop()
        else{
            ::close(channel_->fd());
        }
    }
    else{
        LOG_ERROR("Connector::handleWrite - error: %d", err);
        retry(channel_->fd());
    }
}
//因网络波动等原因断开了自动重连
void Connector::restart(){
    setState(kDisconnected);
    retryDelayMs_ = kInitRetryDelayMs;
    connect_=true;
    startInLoop();
}
void Connector::retry(int sockfd){
    ::close(sockfd);
    setState(kDisconnected);

    if(connect_&&retry_){
        LOG_INFO("Connector::retry - Retry connecting to %s in %d milliseconds", addr_.toIpPort().c_str(),retryDelayMs_);
        loop_->runAfter(retryDelayMs_/1000,[self=shared_from_this()](){
            self->start();
        });
        //指数退避//
        retryDelayMs_=std::min(2*retryDelayMs_,kMaxRetryDelayMs);
    }
    else{
        //如果禁止重试且传入了errorcallback_说明是健康检查，执行CheckerTask传入的errorcallback_快速返回连接状态
        if(!retry_&&errorCallback_){
            loop_->queueInLoop(errorCallback_);
        }
        else if(!connect_)
            LOG_INFO("Connector::retry:do not connect");
    }
}
void Connector::stop(){
    connect_=false;
    loop_->queueInLoop([ptr= shared_from_this()](){
        ptr->stopInLoop();
    });
}
void Connector::stopInLoop(){
    setState(kDisconnected);
    //检查channel_是否存在防止channel_还没被创建出来或者已经被删除掉了导致程序崩溃
    if(channel_){
        channel_->disableAll();
        channel_->remove();
        //确保其它需要用到channel_的任务全都执行完之后才reset
        channel_.reset();
    }
}
//收到对端的错误
void Connector::handleError(){
    int err;
    socklen_t len=sizeof(err);
    int ret =::getsockopt(channel_->fd(),SOL_SOCKET, SO_ERROR, &err, &len);
    if(ret<0){
        err=errno;
    }
    LOG_ERROR("Connector::handleError - SO_ERROR: %d", err);
    retry(channel_->fd());
}