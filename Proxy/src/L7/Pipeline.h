#pragma once
#include <memory>
#include <mutex>
#include <vector>

#include <Vortex-Net/Proxy/L7/Middleware/Middleware.h>

class Pipeline {
public:
    Pipeline():middlewares_(std::make_shared<MiddlewareList>())
    {}

    using MiddlewareList = std::vector<std::shared_ptr<HttpMiddleware>>;
    void addMiddleware(std::shared_ptr<HttpMiddleware> mw) {
        std::unique_lock<std::mutex> lock(mutex_);
        if(middlewares_.use_count()>1){
            middlewares_.reset(new MiddlewareList(*middlewares_));
        }
        middlewares_->push_back(std::move(mw));
    }

    // 顺次执行所有中间件
    bool execute(const TcpConnectionPtr& conn,
                 const std::shared_ptr<HttpContext>& parser,
                 const std::shared_ptr<ClusterManager>& cluster) {
        std::shared_ptr<MiddlewareList> middlewares;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            middlewares=middlewares_;
        }
        
        for (auto& mw : (*middlewares)) {
            if (mw->process(conn, parser, cluster) == MiddlewareAction::Intercept) {
                return false; // 某个中间件拦截了，停止后续执行
            }
        }
        return true; // 全部通过
    }

private:
    std::shared_ptr<MiddlewareList> middlewares_;
    std::mutex mutex_;
};