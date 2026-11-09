#pragma once
#include <Vortex-Net/Proxy/L7/Middleware/CircuitBreaker.h>
#include <Vortex-Net/Proxy/L7/Middleware/Middleware.h>
#include <Vortex-Net/Proxy/L7/ClusterManager.h>

class CircuitBreakerMiddleware : public HttpMiddleware {
public:
    MiddlewareAction process(const TcpConnectionPtr& conn,
                             const std::shared_ptr<HttpContext>& parser,
                             const std::shared_ptr<ClusterManager>& cluster) override {
        if (!cluster || !cluster->breaker) return MiddlewareAction::Continue;

        if (!cluster->breaker->canPass()) {
            const auto resp = CircuitBreaker::serviceUnavailableResponse();
            conn->send(resp.data(), resp.size());
            conn->shutdown();
            return MiddlewareAction::Intercept;
        }
        return MiddlewareAction::Continue;
    }
};