#include <Vortex-Net/TcpClient.h>
#include <Vortex-Net/logger.h>
#include <any>
#include "RomoteClient.h"
#include <memory>
RemoteClient::RemoteClient(EventLoop *loop,
                           InetAddress &serverAddr,
                           std::string &name,
                           TcpConnectionPtr context):
                           tc_(std::make_shared<TcpClient> (loop,serverAddr,name)),
                           loop_(loop),
                           context_(context)
{
    tc_->setConnectionCallback([this](const TcpConnectionPtr &conn){
        onConnection(conn);
    });
    tc_->setMessageCallback([this](const TcpConnectionPtr &conn,
                                            Buffer* buf, 
                                            Timestamp time)
    {
        onMessage(conn,buf,time);
    });    
}
void RemoteClient::onConnection(const TcpConnectionPtr &conn){
    if(conn->connected()){
        
        LOG_INFO("LB_Server:TcpClient connection UP :%s",conn->peerAddress().toIpPort().c_str());

        //冲刷积压数据
        if(pendingBuffer_.readableBytes() > 0)
        {
            conn->send(&pendingBuffer_);
            pendingBuffer_.retrieveAll();
        }
        conn->setContext(context_);
    }
    else{
        LOG_INFO("LB_Server:TcpClient connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        //获取一个指向weak_ptr的指针，更安全的判断any里是否有东西或者存的是否为weak_ptr
        auto* weak_conn_ptr=std::any_cast<std::weak_ptr<TcpConnection>>(&conn->getContext());
        if(weak_conn_ptr)
        {
            //如果提升失败说明LB_Server的conn已经被析构或不存在，不需要进行其它处理（例如客户端还没建立上连接就直接断开了的情况）
            auto shared_conn=weak_conn_ptr->lock();
            if(shared_conn){
                shared_conn->shutdown();
            }
        }
        //无论如何都要重置
        conn->getContext().reset();
    }
}
void RemoteClient::onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time){
    //防止客户端与LB_Server之间的conn_由于客户断开连接析构导致RemoteClient的指针也提前释放
    auto *weak_conn_ptr=std::any_cast<std::weak_ptr<TcpConnection>>(&conn->getContext());
    if(weak_conn_ptr){
        auto shared_conn=weak_conn_ptr->lock();
        if(shared_conn){
            shared_conn->send(buf);
        }
        //说明客户端先断开连接了
        else{
            //提前调用shutdown，节省后续无效接收针对此连接的来自后端服务器的数据
            LOG_INFO("Client is gone, closing backend connection actively.");
            conn->shutdown();
        }
    }
}
void RemoteClient::sendToBackend(Buffer *buf){
    if(!tc_->send(buf)){
        pendingBuffer_.append(buf->peek(),buf->readableBytes());
        buf->retrieveAll();
    }
}