#pragma once
#include "InetAddress.h"
#include "EventLoop.h"
#include "Channel.h"
#include "noncopy.h"
#include "memory"
#include <atomic>
class Connector :NonCopyable ,public std::enable_shared_from_this<Connector>{
public:
    Connector(EventLoop *loop,const InetAddress &serverAddr);
    using NewConnectionCallback=std::function<void (int sockfd)>;
    using ErrorCallback = std::function<void(int saveError)>;
    void setNewConnectionCallback(const NewConnectionCallback& cb){ newConnectionCallback_ = std::move(cb); }
    //设置健康检查时触发的错误回调
    void setErrorCallback(const ErrorCallback& cb){ errorCallback_ = std::move(cb); }
    InetAddress getServerAddr()const{ return addr_;}
    
    //开始请求连接
    void start();
    void restart();
    void stop();
private:
    static const int kInitRetryDelayMs = 500;
    static const int kMaxRetryDelayMs = 30*1000;
    enum StateE{kDisconnected,kConnected,kConnecting};

    
    void startInLoop();
    void stopInLoop();
    void connect();
    void connecting(int sockfd);
    void retry(int sockfd,int saveError=0);

    void handleError();
    void handleWrite();
    void setState(StateE state){state_= state;}

    InetAddress addr_;
    EventLoop *loop_;
    std::atomic_int state_;
    std::atomic_bool connect_;
    std::unique_ptr<Channel> channel_;
    NewConnectionCallback newConnectionCallback_;
    //待
    ErrorCallback errorCallback_;

    int retryDelayMs_;
    bool retry_;

};