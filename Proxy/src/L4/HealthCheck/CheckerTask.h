#pragma once
#include <Vortex-Net/TcpClient.h>
#include <atomic>
#include <optional>
#include <Vortex-Net/Proxy/Common/Selector.h>
class CheckerTask: public std::enable_shared_from_this<CheckerTask>{
public:
    static std::shared_ptr<CheckerTask> create(EventLoop *loop,
                                               std::string &name,
                                               const std::shared_ptr<ServerNode> node);
    
    void start();
    void updateNodeStatus(bool success);
    
private:

    CheckerTask(EventLoop *loop,std::string &name,const std::shared_ptr<ServerNode> node);
    void Init();

    void onConnection(const TcpConnectionPtr &conn);
    void onTimeout();
    void onError(int saveError);

    std::shared_ptr<TcpClient> client_;
    EventLoop *loop_;
    std::optional<TimerId> timerId_;
    std::shared_ptr<ServerNode> node_;
    //用于防止onConnection与定时器触发的冲突 检查过程中只能false->true
    std::atomic<bool> has_determined_;
    //用来标志是否正在进行检查 防止重复发起连接
    std::atomic<bool> is_probing_;
};