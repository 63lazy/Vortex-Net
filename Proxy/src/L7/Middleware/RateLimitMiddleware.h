#pragma once
#include <algorithm>
#include <cstdint>

#include <Vortex-Net/Proxy/L7/Middleware/TokenBucket.h>
#include <Vortex-Net/Proxy/L7/Middleware/Middleware.h>
#include <Vortex-Net/Proxy/L7/ClusterManager.h>

class RateLimitMiddleware : public HttpMiddleware {
public:
    MiddlewareAction process(const TcpConnectionPtr& conn,
                             const std::shared_ptr<HttpContext>& parser,
                             const std::shared_ptr<ClusterManager>& cluster) override {
        if (!cluster || !cluster->limiter) return MiddlewareAction::Continue;

        uint32_t tokens = 1;
        if (parser->request().path.starts_with("/v1")) {
            tokens = std::max(1u, static_cast<uint32_t>(parser->contentLength() / 4));
        }

        if (!cluster->limiter->tryAcquire(tokens)) {
            const auto resp = TokenBucket::tooManyRequestsResponse();
            conn->send(resp.data(), resp.size());
            conn->shutdown();
            return MiddlewareAction::Intercept; // 拦截
        }
        return MiddlewareAction::Continue; // 放行
    }
};