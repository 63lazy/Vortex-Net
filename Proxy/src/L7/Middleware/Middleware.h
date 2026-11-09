#pragma once
#include <memory>
#include <Vortex-Net/TcpConnection.h>
#include <Vortex-Net/Proxy/L7/HttpContext.h>

// 前置声明：ClusterManager 定义在 L7/ClusterManager.h，避免循环包含
struct ClusterManager;

// 中间件处理结果：继续往下走，还是拦截并终止
enum class MiddlewareAction {
    Continue,  // 放行，交给流水线下一个中间件
    Intercept  // 拦截，已经在内部回复了错误报文并断开连接
};

// 抽象基类：所有中间件（安检员）必须实现的接口
class HttpMiddleware {
public:
    virtual ~HttpMiddleware() = default;
    // 返回 true 继续，返回 false 说明被拦截
    virtual MiddlewareAction process(
        const TcpConnectionPtr& conn,
        const std::shared_ptr<HttpContext>& parser,
        const std::shared_ptr<ClusterManager>& cluster) = 0;
};