#include "HttpParser.h"
#include "picohttpparser.h"
#include <Vortex-Net/logger.h>
#include <charconv>
#include <strings.h>

// HTTP请求解析核心实现
bool HttpParser::parseRequest(Buffer *buf, Timestamp receiveTime)
{
    is_req_=true;
    // 零拷贝解析：直接获取缓冲区指针和可读长度
    const char *data = buf->peek();
    size_t len = buf->readableBytes();

    // 解析结果存储变量
    const char *method = nullptr;
    size_t method_len = 0;
    const char *path = nullptr;
    size_t path_len = 0;
    int minor_version = 0;

    // 限制单次请求最大Header数量为100，使用栈上变量避免堆分配
    struct phr_header headers[100];
    size_t num_headers = 100;

    // 调用picohttpparser核心解析函数
    // last_len参数设为0，表示这是一个新的HTTP请求起始位置
    int result = phr_parse_request(
        data, len,
        &method, &method_len,
        &path, &path_len,
        &minor_version,
        headers, &num_headers,
        0  // last_len:前一次未解析完的数据长度，首次解析为0
    );

    // 根据解析结果进行状态处理
    if (result > 0)
    {
        header_len_ = static_cast<size_t>(result); 
        bool has_body=false;
        size_t content_length = 0;
        // 解析成功：返回值为HTTP头部总字节数
        header_.method = std::string(method, method_len);
        header_.path = std::string(path, path_len);

        //预先分配空间，防止 vector 扩容带来的堆重分配开销
        header_.headers.clear();
        header_.headers.reserve(num_headers); 
        //遍历储存头部字段
        for(size_t i=0;i<num_headers;i++){
            std::string_view key(headers[i].name,headers[i].name_len);
            std::string_view val(headers[i].value,headers[i].value_len);

            header_.headers.emplace_back(key,val);
            //如果解析出content_length_ 记录请求总长度
            if(key.size() == 14 && strncasecmp(key.data(), "content-length", 14)==0){
                has_body=true;
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), content_length);
                if (ec == std::errc()) {
                    // 解析成功content_length_ 变量中已经是纯整型数字了
                    LOG_INFO ("Parsed Content-Length: %zu",content_length);
                } 
                else {
                    // 解析失败（比如格式非法或溢出）
                    LOG_ERROR("Invalid Content-Length value!");
                }
                has_body = (content_length > 0);
            }
        }
        //如果有body
        if(has_body){
            content_length_=content_length;
            state_ = State::kExpectBody;
            got_all_ = false;
        }
        else{
            state_ = State::kGotAll;
            got_all_ = true;
        }
        return true;
    }
    else if (result == -2)
    {
        // TCP半包：Http头数据不完整，需要等待更多数据
        // 设置状态为未完成，返回false等待下次数据到达
        got_all_ = false;
        return false;
    }
    else
    {
        // result == -1：协议格式非法
        // 直接返回false，表示请求无效
        buf->retrieveAll();
        LOG_ERROR("HTTP protocol error detected, discarding all buffered data");
        return false;
    }
}

bool HttpParser::parseResponse(Buffer *buf, Timestamp receiveTime)
{
    is_req_=false;
    const char *data = buf->peek();
    size_t len = buf->readableBytes();

    int minor_version = 0;
    int status = 0;
    const char *msg = nullptr;
    size_t msg_len = 0;

    struct phr_header headers[100];
    size_t num_headers = 100;

    int result = phr_parse_response(
        data, len,
        &minor_version, &status,
        &msg, &msg_len,
        headers, &num_headers,
        0
    );

    if (result > 0)
    {
        header_len_ = static_cast<size_t>(result);
        resp_status_ = status;

        bool has_body = false;
        size_t content_length = 0;
        bool is_chunked = false;

        for (size_t i = 0; i < num_headers; i++)
        {
            std::string_view key(headers[i].name, headers[i].name_len);
            std::string_view val(headers[i].value, headers[i].value_len);

            // 比对 Content-Length
            if (key.size() == 14 && strncasecmp(key.data(), "content-length", 14) == 0)
            {
                auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), content_length);
                if (ec == std::errc())
                {
                    LOG_INFO("Response Content-Length: %zu",content_length);
                }
                else
                {
                    LOG_ERROR ("Invalid response Content-Length value!");
                }
                if (content_length > 0)
                {
                    has_body = true;
                }
            }

            // 比对 Transfer-Encoding: chunked
            if (key.size() == 17 && strncasecmp(key.data(), "transfer-encoding", 17) == 0)
            {
                if (val.size() == 7 && strncasecmp(val.data(), "chunked", 7) == 0)
                {
                    is_chunked = true;
                    has_body = true;
                }
            }
        }

        content_length_ = content_length;
        resp_is_chunked_ = is_chunked;

        if (has_body)
        {
            state_ = State::kExpectBody;
            got_all_ = false;
        }
        else
        {
            state_ = State::kGotAll;
            got_all_ = true;
        }

        return true;
    }
    else if (result == -2)
    {
        // 半包：响应头数据不完整，等待更多数据
        got_all_ = false;
        return false;
    }
    else
    {
        // result == -1：协议格式非法
        LOG_ERROR("HTTP response protocol error detected, discarding all buffered data");
        return false;
    }
}

void HttpParser::retrieveHeader(Buffer *buf){
    if(header_len_ > 0){
        buf->retrieve(header_len_); // 消费掉已经转发完的头部
    }
}

void HttpParser::consumeBody(Buffer *buf,size_t n){
    body_bytes_read_+=n;
    if(body_bytes_read_>=content_length_){
        state_=State::kGotAll;
        got_all_ = true;
    }
    buf->retrieve(n);
}

void HttpParser::reset(){
    header_=HttpRequest();
    got_all_=false;
    body_bytes_read_=0;
    header_len_=0;
    content_length_=0;
    resp_is_chunked_=false;
    resp_status_=0;
    state_=State::kExpectHeaders;
}
