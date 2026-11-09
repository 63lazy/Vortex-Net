#include <memory>
#include <string>
#include <Vortex-Net/TcpServer.h>
#include <Vortex-Net/logger.h>
#include <Vortex-Net/Proxy/Common/ServerNode.h>
#include <Vortex-Net/Proxy/L7/RemoteClient.h>
#include <Vortex-Net/Proxy/L7/ConnectionPool.h>
#include <Vortex-Net/Proxy/L7/HttpContext.h>
#include <Vortex-Net/Proxy/L7/Middleware/ApiKeyAuth.h>
#include <Vortex-Net/Proxy/L7/ClusterManager.h>
#include <Vortex-Net/Proxy/L7/Pipeline.h>
#include <Vortex-Net/Proxy/L7/Middleware/AuthMiddleware.h>
#include <Vortex-Net/Proxy/L7/Middleware/RateLimitMiddleware.h>
#include <Vortex-Net/Proxy/L7/Middleware/CircuitBreakerMiddleware.h>
class LB_Server{
public: 
    using remoteClientPtr=std::shared_ptr<RemoteClient>;
    LB_Server(EventLoop *loop,
                const InetAddress &listenAddr,
                const std::string &nameArg,
                std::vector<std::shared_ptr<ServerNode>> serverGroup
                ):
                loop_(loop),
                server_(loop,listenAddr,nameArg),
                im_cluster_(std::make_shared<ClusterManager>(loop, 10000, 5000)),
                serverGroup_(std::move(serverGroup)),
                auth_(),
                PrePipeline_(),
                PostPipeline_()
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
        //构造时仅传入默认Echo集群配置
        for(auto &node:serverGroup_){
            im_cluster_->selector->addNode(node);
        }

        //装配流水线：前置鉴权 → (模型路由) → 后置限流+熔断
        PrePipeline_.addMiddleware(std::make_shared<AuthMiddleware>(auth_));
        PostPipeline_.addMiddleware(std::make_shared<RateLimitMiddleware>());
        PostPipeline_.addMiddleware(std::make_shared<CircuitBreakerMiddleware>());
    }
    void setThreadNum(int n){
        server_.setThreadNum(n);
    }
    void start(){
        server_.start();
    }
    //由用户自己调用来配置其它后端集群
    void addNode(const std::string& model_name,const std::shared_ptr<ServerNode>& node) {
        auto it=route_table_.find(model_name);
        //集群还不存在，新建一个
        if(it==route_table_.end()){
            auto cluster=std::make_shared<ClusterManager>(loop_, 8192, 2048);
            cluster->selector->addNode(node);
            route_table_[model_name] = std::move(cluster); 
        }
        //集群已存在，增加新节点
        else{
            it->second->selector->addNode(node); 
        }
    }
    //用户调用用于初始化默认大模型节点
    void addDefaultLlmNode(const std::shared_ptr<ServerNode>& node) {
        if (!default_llm_cluster_) {
            default_llm_cluster_ = std::make_shared<ClusterManager>(loop_, 8192, 2048);
        }
        default_llm_cluster_->selector->addNode(node);
    }
    void loadKeys(const std::vector<std::string> &keys){
        auth_.load(keys);
    }
    //令牌桶更新
    void updateRateLimit(const std::string& cluster_name, uint32_t new_cap, uint32_t new_rate) {
        if (cluster_name == "llm") {
            if (default_llm_cluster_) default_llm_cluster_->limiter->update(new_cap, new_rate);
        } else if (cluster_name == "im") {
            im_cluster_->limiter->update(new_cap, new_rate);
        }
    }
private:
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            //创建协议解析器并存入conn的context
            std::shared_ptr<HttpContext> parser=std::make_shared<HttpContext>();
            conn->setContext(parser);
            LOG_INFO("LB_Server:TcpServer connection UP :%s",conn->peerAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("LB_Server:TcpServer connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time){
        //线程局部连接池
        thread_local auto localPool=std::make_shared<ConnectionPool>(conn->getLoop());

        auto parser=std::any_cast<std::shared_ptr<HttpContext>>(conn->getContext());
        //期望接收body
        if(parser->isExpectingBody()){
            auto ptr=parser->getClient();
            size_t readable=buf->readableBytes();
            size_t remaining=parser->bytesRemaining();
            //判断可读内容是否都是同一个请求 区分报文边界
            size_t n=std::min(readable,remaining);

            ptr->sendToBackend(buf,n);
            parser->consumeBody(buf,n);
            if(parser->gotAll()){
                parser->reset();
            }
        }
        //成功解析头部
        else if(parser->parseRequest(buf,time)){
            InetAddress addr;
            const std::string& path = parser->request().path;

            //前置流水线：鉴权（仅 /v1 生效，/im 自动放行）；此时尚未确定集群，传 nullptr
            if(!PrePipeline_.execute(conn, parser, nullptr)){
                return; // 鉴权中间件已回 401 并 shutdown
            }

            //确定目标集群与路由键
            std::shared_ptr<ClusterManager> target_cluster;
            std::string route_key;
            if(path.starts_with("/v1")){
                //解析模型名（专属推理集群 > 默认llm集群 > 404）
                route_key = parser->parseModelName(buf);
                auto it=route_table_.find(route_key);
                if(it!=route_table_.end()){
                    target_cluster = it->second;
                }
                else if(default_llm_cluster_){
                    target_cluster = default_llm_cluster_;
                }
                else {
                    //既没有专属集群，也没有默认集群，直接拒绝
                    conn->send("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    conn->shutdown();
                    return;
                }
            }
            else if(path.starts_with("/im") || path.starts_with("/api/im")){
                target_cluster = im_cluster_;
                route_key = path;
            }
            else {
                //未知业务路径，网关直接返回 404 截断
                conn->send("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                conn->shutdown();
                return;
            }

            //后置流水线：限流 → 熔断
            if(!PostPipeline_.execute(conn, parser, target_cluster)){
                return; // 限流/熔断中间件已回 429/503 并 shutdown
            }

            //选路
            addr = std::move(target_cluster->selector->getNextServer(route_key));

            std::string targetAddr=addr.toIpPort();
            
            auto remoteClientPtr = localPool->borrowConnection(targetAddr);
            //绑定熔断器状态回调（每个集群持有独立的 breaker）
            remoteClientPtr->setBackendStatusCallback([cluster = target_cluster](int status_code){
                if(status_code>=200&&status_code<400){
                    cluster->breaker->recordSuccess();
                }else{
                    //4xx/5xx错误
                    cluster->breaker->recordFailure();
                }
            });
            remoteClientPtr->bindClient(conn);
            //用解析器绑定这个后端连接 后续转发消息体时需要用到
            parser->BoundRClient(remoteClientPtr);
            //仅消费掉头部
            size_t header_len=parser->headerLen();
            remoteClientPtr->sendToBackend(buf,header_len);
            if(header_len > 0){
                buf->retrieve(header_len);
            }
            //处理与header同包到达的body部分
            size_t n=std::min(buf->readableBytes(),parser->bytesRemaining());
            if(n>0){
                remoteClientPtr->sendToBackend(buf,n);
                parser->consumeBody(buf,n);
            }
            // 说明没有Body或者body全都在第一个数据包里
            if(parser->gotAll()) {
                parser->reset();
                LOG_INFO("All data packets are sent at once");
            }
        }
    }
    
    EventLoop *loop_;
    TcpServer server_;
    
    //Echo/chat服务集群
    std::shared_ptr<ClusterManager> im_cluster_;
    //默认的llm集群
    std::shared_ptr<ClusterManager> default_llm_cluster_ ;
    //针对大型模型分发 分发到对应的推理集群
    std::unordered_map<std::string,std::shared_ptr<ClusterManager>> route_table_;

    std::vector<std::shared_ptr<ServerNode>> serverGroup_;
    ApiKeyAuthenticator auth_;

    Pipeline PrePipeline_;
    Pipeline PostPipeline_;
};
