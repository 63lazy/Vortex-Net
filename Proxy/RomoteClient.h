#pragma once
#include <Vortex-Net/TcpClient.h>
class RemoteClient:public std::enable_shared_from_this<RemoteClient>{
public:
    RemoteClient(EventLoop *loop,InetAddress &serverAddr,std::string &name,TcpConnectionPtr context);
    void stop(){
        tc_->stop();
    }
    void sendToBackend(Buffer *buf);
    void connect(){tc_->connect();}
private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time);
    std::shared_ptr<TcpClient> tc_;
    EventLoop *loop_;
    std::weak_ptr<TcpConnection> context_;
    Buffer pendingBuffer_;
};