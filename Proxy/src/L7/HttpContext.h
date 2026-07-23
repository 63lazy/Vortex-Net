#pragma once
#include <Vortex-Net/Buffer.h>
#include <Vortex-Net/Timestamp.h>
#include <string_view>
#include <memory>
class RemoteClient;
struct HttpRequest{
    //SBO保证短的sting不会分配堆上内存 栈上内存分配速度 极快可忽略不计
    std::string method;
    std::string path;
    //其它字段可能数量较多 用string_view保证极致的转发性能
    std::vector<std::pair<std::string_view, std::string_view>> headers;
};
 
class HttpContext{
public:
    using remoteClientPtr=std::shared_ptr<RemoteClient>;
    enum class State{
        kExpectHeaders, //初始状态
        kExpectBody,
        kGotAll
    };
    HttpContext():got_all_(false){}
    //解析请求
    bool parseRequest(Buffer *buf,Timestamp recieveTime);
    //解析响应
    bool parseResponse(Buffer *buf,Timestamp recieveTime);
    //状态查询
    bool gotAll() const{return state_==State::kGotAll;}
    bool isExpectingBody(){return state_==State::kExpectBody;}
    bool isExpectingHeader(){return state_==State::kExpectHeaders;}
    //清空缓冲区 在长连接场景下(Keep-Alive)重新迎接下一个请求
    void reset();
    
    //让LB_Server读取请求信息
    const HttpRequest& request() const{return header_;}

    void retrieveHeader(Buffer *buf);
    void BoundRClient(const remoteClientPtr &client){client_= client;}
    remoteClientPtr& getClient(){return client_;}
    void consumeBody(Buffer *buf,size_t n);
    size_t headerLen(){return header_len_ ;}
    //还需要读的字节数
    size_t bytesRemaining(){return content_length_-body_bytes_read_;}
    //分块传输(流式chunked传输)
    bool isResponseChunked(){return resp_is_chunked_;}
    //解析大模型名称
    std::string parseModelName(Buffer* buf);
private:
    //请求字段
    HttpRequest header_;
    bool got_all_; // 记录解析状态
    size_t header_len_=0;
    //响应字段
    int resp_status_ = 0;              // 后端响应状态码（如 200, 502）
    bool resp_is_chunked_ = false;     // 后端响应是否为分块流式传输
    //body长度
    size_t content_length_=0;
    //body已读长度
    size_t body_bytes_read_=0;
    State state_=State::kExpectHeaders;
    
    remoteClientPtr client_;
    bool is_req=true;
};