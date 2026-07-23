#pragma once
#include <Vortex-Net/TcpClient.h>
class RemoteClient:public std::enable_shared_from_this<RemoteClient>{
public:
    using ErrorCallback = std::function<void(const std::shared_ptr<RemoteClient>&)>;
    using ReturnCallback = std::function<void(const std::shared_ptr<RemoteClient>&)>;
    RemoteClient(EventLoop *loop, const InetAddress &serverAddr, const std::string &name);
    void stop(){
        tc_->stop();
    }
    void sendToBackend(Buffer *buf,size_t n);
    //连接后端服务器
    void connect();

    //绑定rclient
    void bindClient(const TcpConnectionPtr &conn){
        clientConn_=conn;
        is_busy_=true;
    }
    //解绑rclient
    void unbind(){
        clientConn_.reset();
        is_busy_=false;
    }
    void setbusy(){is_busy_=true;}
    void setUnbusy(){is_busy_=false;}
    bool is_busy(){return is_busy_;}

    void setErrorCallback(ErrorCallback cb){errorCallback_ = std::move(cb);}
    void setReturnCallback(ReturnCallback cb){returnCallback_ = std::move(cb);}
    void forceclose(){tc_->disconnect();}
private:
    void onConnection(const TcpConnectionPtr &conn);
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time);

    std::shared_ptr<TcpClient> tc_;
    EventLoop *loop_;
    //客户端的弱引用
    std::weak_ptr<TcpConnection> clientConn_;
    Buffer pendingBuffer_;
    //判断改连接是否已被绑定
    bool is_busy_;

    //由连接池传入，用于自毁，清理甚至删除这个RemoteClient对象
    ErrorCallback errorCallback_;
    //调用returnConnection归还连接回到连接池
    ReturnCallback returnCallback_;
};