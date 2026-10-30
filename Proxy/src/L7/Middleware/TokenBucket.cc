#include <Vortex-Net/Proxy/L7/Middleware/TokenBucket.h>

#include <chrono>

namespace {
// 返回相对进程内单调时钟基准的毫秒数（单调不减）
uint32_t nowMs() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point kEpoch = clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - kEpoch).count());
}
} // namespace

TokenBucket::TokenBucket(uint32_t capacity, uint32_t refill_rate_per_sec)
    : config_(Config{capacity, refill_rate_per_sec}),
      state_(State{0, capacity}) {}

bool TokenBucket::tryAcquire(uint32_t need) {
    if(need == 0) return true;

    // 一次 load 拿到一致的容量/速率配置快照，避免 CAS 循环期间配置被切换
    const Config cfg = config_.load(std::memory_order_relaxed);
    if(need > cfg.capacity) return false;

    State cur = state_.load(std::memory_order_relaxed);
    while(true){
        const uint32_t now = nowMs(); // 每次重试都采样最新时间，避免用过时时间计算

        uint32_t tokens = cur.tokens;
        uint32_t ts = cur.timestamp_ms;

        // 距上次结算经过的毫秒可产出的令牌数（整数向下取整）
        const uint64_t produced =
            static_cast<uint64_t>(now - ts) * cfg.refill_rate / 1000;
        // 只有过去时间足够生成至少 1 个令牌才推进时间戳，防止高频微小调用抹除时间片
        if(produced > 0){
            const uint64_t total = static_cast<uint64_t>(tokens) + produced;
            tokens = total >= cfg.capacity ? cfg.capacity : static_cast<uint32_t>(total);
            // 只推进被结算掉的时间，保留不足一个令牌的余数
            ts += static_cast<uint32_t>(produced * 1000 / cfg.refill_rate);
        }

        if(tokens < need){
            return false; // 令牌不足，拒绝（不写回状态，时间片保留在 timestamp_ms 里）
        }

        State next{ts, tokens - need};
        // 乐观重试 CAS：成功用 release 发布新状态，失败用 relaxed 快速重试
        if(state_.compare_exchange_weak(cur, next,
                std::memory_order_release, std::memory_order_relaxed)){
            return true;
        }
        // CAS 失败，cur 已被刷新为最新值，继续下一轮重试
    }
}

uint32_t TokenBucket::availableTokens() const {
    const Config cfg = config_.load(std::memory_order_relaxed);
    State cur = state_.load(std::memory_order_relaxed);
    const uint32_t now = nowMs();
    const uint64_t total = static_cast<uint64_t>(cur.tokens) +
        static_cast<uint64_t>(now - cur.timestamp_ms) * cfg.refill_rate / 1000;
    return total >= cfg.capacity ? cfg.capacity : static_cast<uint32_t>(total);
}

void TokenBucket::update(uint32_t new_capacity, uint32_t new_refill_rate) {
    // 低频写路径，加锁保证绝对串行化，避免结算结果被并发 update 覆盖
    std::lock_guard<std::mutex> lock(update_mutex_);

    const Config old_cfg = config_.load(std::memory_order_relaxed);

    // 先用旧配置把截至调用时刻的令牌结算清楚并写回 state_，再切换新配置
    State cur = state_.load(std::memory_order_relaxed);
    while(true){
        const uint32_t now = nowMs(); // 每次重试都采样最新时间
        
        const uint64_t produced =
            static_cast<uint64_t>(now - cur.timestamp_ms) * old_cfg.refill_rate / 1000;
        uint64_t total = static_cast<uint64_t>(cur.tokens) + produced;
        if(total > old_cfg.capacity) total = old_cfg.capacity; // 旧桶容量上限
        if(total > new_capacity) total = new_capacity;         // 新容量可能更小，防止降容后超限

        State next{now, static_cast<uint32_t>(total)};
        if(state_.compare_exchange_weak(cur, next,
                std::memory_order_release, std::memory_order_relaxed)){
            break;
        }
    }

    config_.store(Config{new_capacity, new_refill_rate}, std::memory_order_release);
}

std::string_view TokenBucket::tooManyRequestsResponse() noexcept {
    static constexpr std::string_view kBody = R"({"error":"Too Many Requests"})";
    static_assert(kBody.size() == 29, "429 错误体长度与 Content-Length 不一致");
    static constexpr char kResponse[] =
        "HTTP/1.1 429 Too Many Requests\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 29\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"error\":\"Too Many Requests\"}";
    return {kResponse, sizeof(kResponse) - 1};
}
