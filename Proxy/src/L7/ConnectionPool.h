#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
class RemoteClient;
class EventLoop;
class TcpConnection;
class ConnectionPool{
public:
    using RemoteClientPtr=std::shared_ptr<RemoteClient>;

    ConnectionPool(EventLoop *loop);

    ConnectionPool(const ConnectionPool&)=delete;
    ConnectionPool& operator=(const ConnectionPool&)=delete;
    //借用连接
    RemoteClientPtr borrowConnection(const std::string &targetNode);
    //归还连接
    void returnConnection(const std::string& targetAddr, const RemoteClientPtr& remoteClient);


private:
    
    EventLoop *loop_;
    //每个节点的连接池
    std::unordered_map<std::string,std::vector<RemoteClientPtr>> idle_map_;

    size_t max_conns_ = 10; // 每个后端节点允许的最大空闲连接数
};  