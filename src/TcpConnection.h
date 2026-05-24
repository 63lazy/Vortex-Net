#pragma once 
#include "noncopy.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"
#include <memory>
#include <any>
#include <string>
#include <atomic>
class Channel; 
class EventLoop;
class Socket;

class TcpConnection : NonCopyable ,public std::enable_shared_from_this<TcpConnection>{
public:
    TcpConnection(EventLoop *loop,
                  const std::string &name,
                  int sockfd,
                  const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();
    EventLoop* getLoop() const {return loop_;}
    //返回引用加const是为了防止不必要的拷贝
    const std::string& name() const {return name_;}
    const InetAddress& localAddress() const {return localAddr_;}
    const InetAddress& peerAddress() const {return peerAddr_;}

    bool connected() const {return state_ == kConnected;}

    void send(const std::string &buf);
    void send(Buffer *buf);
    void shutdown();
    void forceclose();

    void setConnectionCallback(const ConnectionCallback &cb){connectionCallback_ = cb;}
    void setMessageCallback(const MessageCallback &cb){messageCallback_ = cb;}
    void setWriteCompleteCallback(const WriteCompleteCallback &cb){writeCompleteCallback_ = cb;}
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb,size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_=highWaterMark;
    }

    void setCloseCallback(const CloseCallback& cb){ closeCallback_ = cb; }

    //连接建立
    void connectEstablished();
    //连接销毁
    void connectDestroyed();

    //获取最后一次活跃的时间戳
    int64_t lastReceiveTime() const { return lastReceiveTime_.load(); }

    //获取context
    void setContext(const std::any context){context_ = context;}  
    std::any &getContext(){return context_;}
private:

    enum StateE{kDisconnected,kConnecting,kConnected,kDisconnecting};
    void setState(StateE state){state_= state;}

    void handleRead(Timestamp recieveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void * data,size_t len);
    void shutdownInLoop();
    void forcecloseInLoop();
    
    EventLoop *loop_;
    std::string name_;
    std::atomic_int state_;
    bool reading_;

    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    
    //连接改变的回调，包括建立连接和断开连接
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;   
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_; 

    size_t highWaterMark_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;
    //记录最后活跃的时间
    std::atomic<int64_t> lastReceiveTime_; 

    std::any context_;
};