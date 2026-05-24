#pragma once
#include "socket.h"
#include "noncopy.h"
#include "EventLoop.h"
#include "Callbacks.h"
#include "Connector.h"
#include "logger.h"
#include "TcpConnection.h"
#include <atomic>
#include <memory>
class TcpClient :public NonCopyable,public std::enable_shared_from_this<TcpClient>{
public:
    using ErrorCallback = std::function<void(int saveError)>;
    TcpClient(EventLoop *loop,const InetAddress &serverAddr,std::string &name);
    void connect();
    void disconnect();
    void stop();
    void enableRetry(){retry_ = true;}
    
    void setConnectionCallback(const ConnectionCallback cb){connectionCallback_ = std::move(cb);}
    //提供给上层的接口//
    void setErrorCallback(const ErrorCallback& cb){ connector_->setErrorCallback(cb); }
    void setMessageCallback(const MessageCallback cb){messageCallback_ = std::move(cb);}
    void setWriteCompleteCallback(const WriteCompleteCallback cb){writeCompleteCallback_ = std::move(cb);}

    bool send(Buffer *buf);
private:
    void newConnection(int sockfd);
    void removeConnection(const TcpConnectionPtr &conn);
    
    ConnectionCallback connectionCallback_;
    //用于健康检查的errorcallback
    ErrorCallback errorCallback_;
    MessageCallback messageCallback_;   
    WriteCompleteCallback writeCompleteCallback_;
    
    EventLoop *loop_;
    std::shared_ptr<Connector> connector_;
    TcpConnectionPtr conn_;

    std::string name_;
    int nexConnId_;
    std::mutex mutex_;
    std::atomic_bool retry_;
    std::atomic_bool connect_;

};
