#pragma once
// ApiKeyAuth.h —— 无锁、纯内存的 API Key 鉴权中间件（写时复制风格）

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ApiKeyAuthenticator {
public:
    using KeySet = std::vector<std::string>; // 有序（升序），配合透明比较器二分查找

    ApiKeyAuthenticator();
    explicit ApiKeyAuthenticator(std::vector<std::string> keys);

    ApiKeyAuthenticator(const ApiKeyAuthenticator&) = delete;
    ApiKeyAuthenticator& operator=(const ApiKeyAuthenticator&) = delete;

    // 全量替换 API Key 集合
    void load(std::vector<std::string> keys);
    // 新增单个 key（幂等）
    void addKey(std::string key);
    // 删除单个 key（幂等）
    void removeKey(std::string_view key);

    // 校验 Authorization 原始字符串，例如 "Bearer <token>"
    bool authenticate(std::string_view auth_header_value) const;
    // 校验 HTTP 头键值对集合（自动定位 Authorization 头，名称大小写不敏感）
    bool authenticate(
        const std::vector<std::pair<std::string_view, std::string_view>>& headers) const;

    // 快速构造硬编码 HTTP/1.1 401 Unauthorized 响应（含 Connection: close 与简短 JSON 错误体）
    static std::string_view unauthorizedResponse() noexcept;

private:
    // 配合写时复制使用
    std::shared_ptr<KeySet> keys_;
    mutable std::mutex mutex_;
};
