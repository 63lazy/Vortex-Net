#include "TcpClient.h"
#include "Connector.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "logger.h"
#include <strings.h>
#include <string>
static EventLoop *CheckloopNotNull(EventLoop *loop){
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection loop is null!",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}
TcpClient::TcpClient(EventLoop *loop,const InetAddress &serverAddr,std::string &name):
                     loop_(CheckloopNotNull(loop)),
                     connector_(std::make_shared<Connector>(loop,serverAddr)),
                     name_(name),
                     nexConnId_(1),
                     retry_(false),
                     connect_(true)
{
    connector_->setNewConnectionCallback([this](int sockfd){
        newConnection(sockfd);
    });
}

void TcpClient::connect(){
    connect_=true;
    connector_->start();
}
//与stop相比不禁用connector的连接与重连能力，仅仅断开当前连接
void TcpClient::disconnect(){
    connect_=false;
    //防止同时有handleCloes的调用删除了conn_对象
    TcpConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn=conn_;
    }
    conn->shutdown();
}

void TcpClient::newConnection(int sockfd){
    char buf[64]={0};
    //通过connector_获取服务端的InetAddress
    InetAddress peerAddr=connector_->getServerAddr();
    snprintf(buf,sizeof(buf),"%s#%d",peerAddr.toIpPort().c_str(),nexConnId_);
    nexConnId_++;
    std::string connName=name_ + buf;
    LOG_INFO("TcpClient::newConnection[%s] new connection [%s] from %s",
            name_.c_str(),connName.c_str(),peerAddr.toIpPort().c_str());
    //通过sockfd获取其绑定的本机ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local,sizeof(local));
    socklen_t addrlen=sizeof(struct sockaddr_in);
    if(::getsockname(sockfd,(sockaddr*)&local,&addrlen)<0)
    {
        LOG_ERROR("TcpClient::getLocalAddr");
    }
    InetAddress localAddr(local);
    
    //先在栈上准备好TcpConnection对象再交给TcpClient
    TcpConnectionPtr conn=std::make_shared<TcpConnection>(loop_,connName,sockfd,localAddr,peerAddr);

    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    auto ptr=shared_from_this();
    conn->setCloseCallback([ptr](const TcpConnectionPtr &conn){
        ptr->removeConnection(conn);
    });
    conn->connectEstablished();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn_=conn;
    }
}

void TcpClient::removeConnection(const TcpConnectionPtr &conn){
    LOG_INFO("TcpClient::removeConnection [%s]",
             name_.c_str());
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn_.reset();
    }
    
    loop_->queueInLoop([this, conn](){
        conn->connectDestroyed();
    });
    //如果用户设置了retry_标志 且客户端没有手动stop()
    if(retry_&&connect_){
        LOG_INFO("TcpClient::connect[%s] - Reconnecting to %s",name_.c_str(),connector_->getServerAddr().toIpPort().c_str());
        connector_->restart();
    }
}

void TcpClient::stop(){
    connect_=false;
    connector_->stop();

    //加锁防止removeConnection同时在访问conn_
    TcpConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        conn=conn_;
    }
    if(conn)
        conn->shutdown();
}

bool TcpClient::send(Buffer *buf){ 
    TcpConnectionPtr conn;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        conn=conn_;
    }
    if(conn){
        conn->send(buf); 
        return true;
    }
    else
    {
        return false;
    }
}