#pragma once
#include <cstdint>
#include <memory>

#include <Vortex-Net/Proxy/Common/Selector.h>
#include <Vortex-Net/Proxy/L7/Middleware/TokenBucket.h>
#include <Vortex-Net/Proxy/L7/Middleware/CircuitBreaker.h>

// 集群治理单元：聚合各集群独立的选路器 / 熔断器 / 限流器
struct ClusterManager {
    std::shared_ptr<Selector> selector;         // 负载均衡选路器
    std::shared_ptr<CircuitBreaker> breaker;     // 专属熔断器
    std::shared_ptr<TokenBucket> limiter;       // 专属令牌桶限流器

    ClusterManager(EventLoop* loop, uint32_t capacity, uint32_t rate)
        : selector(std::make_shared<Selector>(loop)),
          breaker(std::make_shared<CircuitBreaker>()),
          limiter(std::make_shared<TokenBucket>(capacity, rate)) {}
};