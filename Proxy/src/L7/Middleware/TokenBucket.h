#include<cstdint>
#include<atomic>
#include <string_view>
#include <mutex>

struct alignas(8) State{
    uint32_t timestamp_ms{0};
    uint32_t tokens;
};

// 打包容量与补充速率，避免两个独立原子变量并发读时出现配置撕裂
struct alignas(8) Config{
    uint32_t capacity;
    uint32_t refill_rate;
};

class TokenBucket{
public:
    TokenBucket(uint32_t capacity_,uint32_t refill_rate_per_sec);

    TokenBucket(const TokenBucket&)=delete;
    TokenBucket& operator=(const TokenBucket&) = delete;

    // 核心限流接口
    bool tryAcquire(uint32_t tokens = 1);

    // 状态观测与 429 快捷报文
    uint32_t availableTokens() const;
    static std::string_view tooManyRequestsResponse() noexcept;

    void update(uint32_t new_capacity,uint32_t new_refill_rate);
private:
    std::atomic<Config> config_;
    std::atomic<State> state_;
    std::mutex update_mutex_;
};