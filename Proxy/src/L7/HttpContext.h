#pragma once
#include <Vortex-Net/Http/HttpParser.h>
#include <Vortex-Net/Buffer.h>
#include <Vortex-Net/Timestamp.h>
#include <string_view>
#include <memory>

class RemoteClient;

// 网关专用 HTTP 上下文：组合纯解析器 HttpParser + 后端连接 client_
class HttpContext{
public:
    using remoteClientPtr=std::shared_ptr<RemoteClient>;

    // ---- 以下接口全量转发给内部 HttpParser ----
    bool parseRequest(Buffer *buf,Timestamp recieveTime){ return parser_.parseRequest(buf, recieveTime); }
    bool parseResponse(Buffer *buf,Timestamp recieveTime){ return parser_.parseResponse(buf, recieveTime); }
    bool gotAll() const{ return parser_.gotAll(); }
    bool isExpectingBody(){ return parser_.isExpectingBody(); }
    bool isExpectingHeader(){ return parser_.isExpectingHeader(); }
    const HttpRequest& request() const{ return parser_.request(); }
    void retrieveHeader(Buffer *buf){ parser_.retrieveHeader(buf); }
    void consumeBody(Buffer *buf,size_t n){ parser_.consumeBody(buf, n); }
    size_t headerLen(){ return parser_.headerLen(); }
    size_t bytesRemaining(){ return parser_.bytesRemaining(); }
    bool isResponseChunked(){ return parser_.isResponseChunked(); }
    const HttpRequest& request(){return parser_.request();}

    // 组合 reset：先重置解析器；解析的是响应时再释放后端连接
    void reset(){
        parser_.reset();
        if(!parser_.isRequest())
            client_.reset();
    }

    //网关特有：后端连接绑定与模型路由
    void BoundRClient(const remoteClientPtr &client){ client_= client; }
    remoteClientPtr& getClient(){ return client_; }

    // 解析大模型名称（模型路由，属于网关业务而非通用协议解析）
    std::string parseModelName(Buffer* buf);

private:
    HttpParser parser_;
    remoteClientPtr client_;
};
