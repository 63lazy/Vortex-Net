#include <Vortex-Net/Proxy/L7/Middleware/ApiKeyAuth.h>

#include <algorithm>

namespace {

// 透明比较器：允许用 std::string_view 直接与 std::string 元素比较，零拷贝
struct KeyLess {
    using is_transparent = void;
    bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
        return lhs < rhs;
    }
};

constexpr bool isSpace(char c) noexcept {
    return c == ' ' || c == '\t';
}

constexpr char toLower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

std::string_view trimLeft(std::string_view s) noexcept {
    size_t i = 0;
    while(i < s.size() && isSpace(s[i])) ++i;
    return s.substr(i);
}

std::string_view trimRight(std::string_view s) noexcept {
    size_t n = s.size();
    while(n > 0 && isSpace(s[n - 1])) --n;
    return s.substr(0, n);
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if(a.size() != b.size()) return false;
    for(size_t i = 0; i < a.size(); ++i){
        if(toLower(a[i]) != toLower(b[i])) return false;
    }
    return true;
}

// 提取 Bearer 后的 token：大小写不敏感 + 前后空白修剪；格式非法返回空
std::string_view extractToken(std::string_view header) noexcept {
    header = trimLeft(header);

    constexpr std::string_view kScheme = "bearer";
    if(header.size() < kScheme.size()) return {};
    for(size_t i = 0; i < kScheme.size(); ++i){
        if(toLower(header[i]) != kScheme[i]) return {};
    }
    header.remove_prefix(kScheme.size());

    header = trimLeft(header);   // 跳过 Bearer 与 token 之间的空白
    header = trimRight(header);  // 去掉 token 尾部空白
    return header;
}

} // namespace

ApiKeyAuthenticator::ApiKeyAuthenticator(): keys_(std::make_shared<KeySet>()) {}

ApiKeyAuthenticator::ApiKeyAuthenticator(std::vector<std::string> keys)
    : ApiKeyAuthenticator() {
    load(std::move(keys));
}

void ApiKeyAuthenticator::load(std::vector<std::string> keys) {
    std::sort(keys.begin(), keys.end());
    // 去重操作
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    std::unique_lock<std::mutex> lock(mutex_);
    keys_.reset(new KeySet(std::move(keys)));
}

void ApiKeyAuthenticator::addKey(std::string key) {
    std::unique_lock<std::mutex> lock(mutex_);
    // 说明此时有线程在读，写时复制
    if(keys_.use_count() > 1){
        keys_.reset(new KeySet(*keys_));
    }
    auto it = std::lower_bound(keys_->begin(), keys_->end(), key);
    if(it != keys_->end() && *it == key) return; // 已存在，幂等
    keys_->insert(it, std::move(key));
}

void ApiKeyAuthenticator::removeKey(std::string_view key) {
    std::unique_lock<std::mutex> lock(mutex_);
    if(keys_.use_count() > 1){
        keys_.reset(new KeySet(*keys_));
    }
    auto &nodes = *keys_;
    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
        [&](const std::string& k){ return std::string_view(k) == key; }),
        nodes.end());
}

bool ApiKeyAuthenticator::authenticate(std::string_view auth_header_value) const {
    std::string_view token = extractToken(auth_header_value);
    if(token.empty()) return false;

    // 高频读操作，短暂加锁取快照副本，之后无锁二分查找
    std::shared_ptr<KeySet> keys;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        keys = keys_;
    }
    return std::binary_search(keys->begin(), keys->end(), token, KeyLess{});
}

bool ApiKeyAuthenticator::authenticate(
    const std::vector<std::pair<std::string_view, std::string_view>>& headers) const {
    for(const auto& [name, value] : headers){
        if(equalsIgnoreCase(name, "Authorization")){
            return authenticate(value);
        }
    }
    return false;
}

std::string_view ApiKeyAuthenticator::unauthorizedResponse() noexcept {
    static constexpr std::string_view kBody = R"({"error":"Unauthorized"})";
    static_assert(kBody.size() == 24, "401 错误体长度与 Content-Length 不一致");
    static constexpr char kResponse[] =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 24\r\n"
        "Connection: close\r\n"
        "\r\n"
        "{\"error\":\"Unauthorized\"}";
    return {kResponse, sizeof(kResponse) - 1};
}
