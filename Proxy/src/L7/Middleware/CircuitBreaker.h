#pragma once
#include <atomic>
#include <string_view>
#include <mutex>

class CircuitBreaker{
public:
    enum class State{open,half_open,close};
    CircuitBreaker(int failure=3,int cool_down_ms=5000);

    bool canPass();

    void recordSuccess();
    void recordFailure();
    //快速构造.rodata 只读的 HTTP/1.1 503 Service Unavailable报文
    static std::string_view serviceUnavailableResponse() noexcept;
private:
    mutable std::mutex mutex_;

    int failure_standard_;
    int cool_down_ms_;

    uint64_t last_open_time_;
    int failure_count_{0};
    State state_{State::close};
};