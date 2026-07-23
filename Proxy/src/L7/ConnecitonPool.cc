#include "ConnectionPool.h"
#include "RemoteClient.h"
#include <Vortex-Net/InetAddress.h>
#include <Vortex-Net/Callbacks.h>

ConnectionPool::ConnectionPool(EventLoop *loop)
    : loop_(loop)
{}

ConnectionPool::RemoteClientPtr ConnectionPool::borrowConnection(const std::string &targetNode)
{
    auto it = idle_map_.find(targetNode);
    if (it != idle_map_.end() && !it->second.empty())
    {
        // 有空闲连接，从vector尾部弹出
        RemoteClientPtr client = std::move(it->second.back());
        it->second.pop_back();
        client->setbusy();
        return client;
    }

    // 没有空闲连接，创建新的RemoteClient
    // 从targetNode解析ip和port，格式: "ip:port"
    size_t colonPos = targetNode.rfind(':');
    std::string ip = "127.0.0.1";
    uint16_t port = 0;
    if (colonPos != std::string::npos)
    {
        ip = targetNode.substr(0, colonPos);
        port = static_cast<uint16_t>(std::stoi(targetNode.substr(colonPos + 1)));
    }

    InetAddress serverAddr(port, ip);
    auto client = std::make_shared<RemoteClient>(loop_, serverAddr, targetNode);

    //直接捕获this 连接池是thread_local的 只要线程还在就不会被销毁
    client->setErrorCallback([this,targetNode](const std::shared_ptr<RemoteClient>& removeptr){
        auto it = this->idle_map_.find(targetNode);
        //如果该连接正在忙 也就是RemoteClient不在连接池里 用std::erase可以实现优雅删除
        if(it!=this->idle_map_.end()){
            std::erase(it->second,removeptr);
        }
    });
    client->setReturnCallback([this,targetNode](const std::shared_ptr<RemoteClient>& returnptr){
        this->returnConnection(targetNode,returnptr);
    });
    client->connect();  // 发起向后端服务器的连接
    client->setbusy();
    LOG_INFO("Borrow Connection success");
    return client;
}

void ConnectionPool::returnConnection(const std::string& targetAddr,
                                       const ConnectionPool::RemoteClientPtr& remoteClient)
{
    remoteClient->unbind();
    remoteClient->setUnbusy();

    auto &vec = idle_map_[targetAddr];
    if (vec.size() < max_conns_)
    {
        // 未达上限，归还到空闲池
        vec.push_back(remoteClient);
    }
    // 达到上限则直接析构（remoteClient引用计数归零后自动销毁）
    else{
        remoteClient->forceclose();
    }
}
