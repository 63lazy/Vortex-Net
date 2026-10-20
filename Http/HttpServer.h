#include<Vortex-Net/Http/HttpParser.h>
#include<Vortex-Net/logger.h>
#include<Vortex-Net/TcpServer.h>
#include<algorithm>
#include<any>
#include<functional>
#include<memory>
#include<string>
#include<unordered_map>

class HttpServer{
public:
    
    using RequestCallback = std::function<void(const TcpConnectionPtr&,
                                            const HttpRequest&,
                                            const std::string& body)>;
    void setRequestCallback(RequestCallback cb) { onRequest_ = std::move(cb); }

    // 快速构造并发送一个完整的非流式 HTTP 响应，业务回调里直接调用
    // HttpServer::respond(conn, 200, "...") 即可，无需手拼响应头。
    static void respond(const TcpConnectionPtr &conn,
                        int status,
                        const std::string &body,
                        const std::string &contentType = "application/json") {
        static const std::unordered_map<int, std::string> reasonMap = {
            {200, "OK"},           {201, "Created"},
            {204, "No Content"},   {400, "Bad Request"},
            {401, "Unauthorized"}, {404, "Not Found"},
            {405, "Method Not Allowed"},
            {500, "Internal Server Error"},
            {503, "Service Unavailable"},
        };
        std::string reason = "Unknown";
        auto it = reasonMap.find(status);
        if (it != reasonMap.end()) reason = it->second;

        std::string resp;
        resp.reserve(body.size() + 256);
        resp += "HTTP/1.1 " + std::to_string(status) + " " + reason + "\r\n";
        resp += "Content-Type: " + contentType + "\r\n";
        resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
        resp += "Connection: keep-alive\r\n";
        resp += "Access-Control-Allow-Origin: *\r\n";
        resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        resp += "Access-Control-Allow-Headers: Content-Type\r\n\r\n";
        resp += body;
        conn->send(resp);
    }

    HttpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg):
              server_(loop,listenAddr,nameArg)
    {
        server_.setConnectionCallback([this](const TcpConnectionPtr &conn){
            onConnection(conn);
        });
        server_.setMessageCallback([this](const TcpConnectionPtr &conn,
                                            Buffer* buf, 
                                            Timestamp time)
        {
            onMessage(conn,buf,time);
        });
    }
    void start(){
        server_.start();
    }
    void setThreadNums(int nums){
        server_.setThreadNum(nums);
    }
private:
    struct ConnState { //每个连接调度解析器 + body累积区 
        std::shared_ptr<HttpParser> parser = std:: make_shared <HttpParser>();
        std::string body;
    };
    TcpServer server_;
    RequestCallback onRequest_;
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected()){
            conn->setContext(std::make_shared<ConnState>());
            LOG_INFO("connection UP :%s",conn->peerAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer* buf, 
                   Timestamp time)
    {
        auto st=std::any_cast<std::shared_ptr<ConnState>>(conn->getContext()); 
        auto &parser = st->parser;
        if(parser->isExpectingBody()){
            collectBody(conn,st,buf);
            return ;
        }
        else if(parser->parseRequest(buf,time)){
            //消费头部
            size_t headerlen=parser->headerLen();
            if(headerlen > 0){
                buf->retrieve(headerlen);
            }
            //collectBody函数处理剩下的部分
            if(parser->isExpectingBody()) collectBody(conn,st,buf);
            else dispatch(conn,st);
        }
    }
    void collectBody(const TcpConnectionPtr &conn, 
                     const std::shared_ptr<ConnState> &st,
                     Buffer *buf)
    {
        auto &parser=st->parser;
        size_t n=std::min(parser->bytesRemaining(),buf->readableBytes());
        if(n>0){
            st->body.append(buf->peek(),n);
            parser->consumeBody(buf,n);
        }
        if(parser->gotAll()) dispatch(conn,st);
    }
    //业务分发
    void dispatch(const TcpConnectionPtr &conn, 
                  const std::shared_ptr<ConnState> &st)
    {
        if(onRequest_) onRequest_(conn,st->parser->request(),st->body);
        st->body.clear();
        st->parser->reset();
    }
};