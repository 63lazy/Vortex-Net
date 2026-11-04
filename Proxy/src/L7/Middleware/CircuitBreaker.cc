#include <Vortex-Net/Proxy/L7/Middleware/CircuitBreaker.h>

#include <chrono>

namespace {
// 返回相对进程内单调时钟基准的毫秒数
uint64_t nowMs() {
    using clock = std::chrono::steady_clock;
    static const clock::time_point kEpoch = clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - kEpoch).count());
}
} // namespace

CircuitBreaker::CircuitBreaker(int failure, int cool_down_ms)
    : failure_standard_(failure),
      cool_down_ms_(cool_down_ms),
      last_open_time_(0) {}

bool CircuitBreaker::canPass() {
    std::lock_guard<std::mutex> lock(mutex_);

    if(state_ == State::close){
        return true;
    }

    if(state_ == State::open){
        // 惰性检查冷却期是否已过
        const uint64_t now = nowMs();
        if(now - last_open_time_ >= static_cast<uint64_t>(cool_down_ms_)){
            state_ = State::half_open; // 推进为半开，放行本次唯一试探请求
            return true;
        }
        return false; // 冷却期内继续熔断
    }

    // half_open：已有先锋试探请求在途，后继请求全部拦截，防惊群
    return false;
}

void CircuitBreaker::recordSuccess() {
    std::lock_guard<std::mutex> lock(mutex_);

    if(state_ == State::half_open){
        state_ = State::close; // 试探成功，熔断器闭合
        failure_count_ = 0;
    }
    else if(state_ == State::close){
        failure_count_ = 0; // 成功一次，清零连续失败计数
    }
    // open 状态下不会产生成功回调，忽略
}

void CircuitBreaker::recordFailure() {
    std::lock_guard<std::mutex> lock(mutex_);

    if(state_ == State::close){
        ++failure_count_;
        if(failure_count_ >= failure_standard_){
            state_ = State::open; // 连续失败达标，跳闸
            last_open_time_ = nowMs();
        }
    }
    else if(state_ == State::half_open){
        state_ = State::open; // 试探失败，立即跌回 open 并重新进入冷却期
        last_open_time_ = nowMs();
    }
    // open 状态下继续记录失败无意义，忽略
}

std::string_view CircuitBreaker::serviceUnavailableResponse() noexcept {
    static constexpr std::string_view kBody = R"({"error":"Service Unavailable"})";
    static_assert(kBody.size() == 31, "503 错误体长度与 Content-Length 不一致");
    static constexpr char kResponse[] =
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 31\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"error\":\"Service Unavailable\"}";
    return {kResponse, sizeof(kResponse) - 1};
}
