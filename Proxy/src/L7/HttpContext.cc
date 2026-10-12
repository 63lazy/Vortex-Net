#include "HttpContext.h"
#include <Vortex-Net/logger.h>
#include <string_view>

// 模型路由：从请求 body 中提取 "model" 字段（网关业务，非通用协议解析）
std::string HttpContext::parseModelName(Buffer* buf) {
    // 零拷贝视口：直接在原始缓冲区内搜索（跳过已解析的头部）
    std::string_view body(buf->peek() + headerLen(), buf->readableBytes() - headerLen());

    // 快速定位model字段
    size_t pos = body.find("\"model\"");
    if (pos != std::string_view::npos) {
        // 定位冒号 :
        size_t colon = body.find(":", pos);
        if (colon != std::string_view::npos) {
            // 定位 model 值左边的双引号 "
            size_t start_quote = body.find("\"", colon);
            if (start_quote != std::string_view::npos) {
                // 定位 model 值右边的双引号 "
                size_t end_quote = body.find("\"", start_quote + 1);
                if (end_quote != std::string_view::npos) {
                    // 完美提取出模型名称（如 "qwen2.5:1.5b"）并转化为 std::string 返回
                    return std::string(body.substr(start_quote + 1, end_quote - start_quote - 1));
                }
            }
        }
    }
    return ""; // 如果没有找到（格式非法），返回空
}
