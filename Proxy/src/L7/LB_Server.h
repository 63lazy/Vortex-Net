#include <memory>
#include<string>
#include <Vortex-Net/TcpServer.h>
#include <Vortex-Net/logger.h>
#include <Vortex-Net/Proxy/Common/ServerNode.h>
#include <Vortex-Net/Proxy/Common/Selector.h>
#include <Vortex-Net/Proxy/L7/RemoteClient.h>
#include <Vortex-Net/Proxy/L7/ConnectionPool.h>
#include <Vortex-Net/Proxy/L7/HttpContext.h>
#include <Vortex-Net/Proxy/L7/Middleware/ApiKeyAuth.h>
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
                im_selector_(std::make_shared<Selector>(loop)),
                serverGroup_(std::move(serverGroup)),
                auth_()
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
            im_selector_->addNode(node);
        }
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
            auto selector=std::make_shared<Selector>(loop_);
            selector->addNode(node);
            route_table_[model_name] = std::move(selector); 
        }
        //集群已存在，增加新节点
        else{
            it->second->addNode(node); 
        }
    }
    //用户调用用于初始化默认大模型节点
    void addDefaultLlmNode(const std::shared_ptr<ServerNode>& node) {
        if (!default_llm_selector_) {
            default_llm_selector_ = std::make_shared<Selector>(loop_);
        }
        default_llm_selector_->addNode(node);
    }
    void loadKeys(const std::vector<std::string> &keys){
        auth_.load(keys);
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
            std::shared_ptr<Selector> target_selector;
            InetAddress addr;
            if(parser->request().path.starts_with("/v1"))
            {
                
                if(!auth_.authenticate(parser->request().headers)){
                    const auto resp = ApiKeyAuthenticator::unauthorizedResponse();
                    conn->send(resp.data(), resp.size());
                    conn->shutdown();
                    return;
                }
                std::string model_name = parser->parseModelName(buf);
                auto it=route_table_.find(model_name);
                if(it!=route_table_.end()) target_selector=it->second;
                else if(default_llm_selector_) target_selector=default_llm_selector_;
                else {
                    //既没有专属集群，也没有默认集群，直接拒绝
                    conn->send("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    conn->shutdown();
                    return;
                }
                addr=std::move(target_selector->getNextServer(model_name));
            }
            else if(parser->request().path.starts_with("/im")){
                target_selector=im_selector_;
                addr = std::move(target_selector->getNextServer(parser->request().path));
            }
            else {
                //未知业务路径，网关直接返回 404 截断
                conn->send("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                conn->shutdown();
                return;
            }
            std::string targetAddr=addr.toIpPort();
            
            auto remoteClientPtr = localPool->borrowConnection(targetAddr);
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
    
    //Echo/chat服务
    std::shared_ptr<Selector> im_selector_;
    //默认的llm集群
    std::shared_ptr<Selector> default_llm_selector_ ;
    //针对大型模型分发 分发到对应的推理集群
    std::unordered_map<std::string,std::shared_ptr<Selector>> route_table_;

    std::vector<std::shared_ptr<ServerNode>> serverGroup_;
    ApiKeyAuthenticator auth_;

};
