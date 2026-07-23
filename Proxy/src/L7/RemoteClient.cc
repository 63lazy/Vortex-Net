#include <Vortex-Net/TcpClient.h>
#include <Vortex-Net/logger.h>
#include <any>
#include <memory>
#include "HttpContext.h"
#include "RemoteClient.h"
RemoteClient::RemoteClient(EventLoop *loop,
                           const InetAddress &serverAddr,
                           const std::string &name):
                           tc_(std::make_shared<TcpClient>(loop,serverAddr,name)),
                           loop_(loop)
{
    auto weakptr=weak_from_this();
    tc_->setConnectionCallback([weakptr](const TcpConnectionPtr &conn){
        if(auto self=weakptr.lock())
            self->onConnection(conn);
    });
    tc_->setMessageCallback([weakptr](const TcpConnectionPtr &conn,
                                            Buffer* buf,
                                            Timestamp time)
    {
        if(auto self=weakptr.lock())
            self->onMessage(conn,buf,time);
    });
}
void RemoteClient::onConnection(const TcpConnectionPtr &conn){
    if(conn->connected()){
        //连上时就已经绑定了客户端 说明是懒加载产生的
        if(!clientConn_.expired()){
            //冲刷积压数据
            if(pendingBuffer_.readableBytes() > 0)
            {
                conn->send(pendingBuffer_.peek(),pendingBuffer_.readableBytes());
                pendingBuffer_.retrieveAll();
            }
        }
    }
    //连接意外断开/线程池主动老化断开
    else{
        //正在执行任务时断开 异常
        if(is_busy_){
            auto client=clientConn_.lock();
            if(client){
                //向客户端发送502
                client->send("HTTP/1.1 502 Bad Gateway\r\n\r\n");
                client->shutdown();
            }
        }

        //连接池统一回收连接对象
        //连接池只需要释放可能存在池子里的该连接对象的强引用
        //因为仅剩的可能存在的另一个引用在客户端的conn里，而它已经因为异常断开随着conn析构了而释放了
        if(errorCallback_){
            auto self=shared_from_this();
            loop_->queueInLoop([self](){
                self->errorCallback_(self);
            });
        }
    }
}
void RemoteClient::onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp time){
    //防止客户端与LB_Server之间的conn_由于客户断开连接析构导致RemoteClient的指针也提前释放
    auto client_conn_ptr=clientConn_.lock();
    if(client_conn_ptr && is_busy_){
        auto parser=std::any_cast<std::shared_ptr<HttpContext>>(client_conn_ptr->getContext());
        if(parser->isExpectingBody()){
            //非流式数据
            if(!parser->isResponseChunked()){
                if(!parser->gotAll()){
                    size_t readalble=buf->readableBytes();
                    size_t remaining=parser->bytesRemaining();
                    //判断可读内容是否都是本次响应的
                    //便于后续扩展一个parser同时处理多个请求的响应
                    size_t n=std::min(readalble,remaining);

                    client_conn_ptr->send(buf->peek(),n);
                    parser->consumeBody(buf,n);
                    if(parser->gotAll()&&returnCallback_){
                        auto self=shared_from_this();
                        parser->reset();
                        returnCallback_(self);
                    }
                }
            }
            //分块传输（流式 Chunked 传输）
            else{
                std::string_view payload(buf->peek(), buf->readableBytes());
                bool finished=payload.find("0\r\n\r\n") != std::string_view::npos;
                client_conn_ptr->send(buf);
                if (finished){
                    LOG_INFO ("Stream finished (Zero-Chunk detected).");
                    //续命防止reset将remoteclient释放之后，returnCallback_执行时访问remoteclient的野指针
                    auto self=shared_from_this();
                    parser->reset();
                    if (returnCallback_) returnCallback_(self);
                }
            }
        }
        //成功解析头部
        else if(parser->parseResponse(buf,time)){
            //仅消费掉头部
            size_t header_len=parser->headerLen();
            client_conn_ptr->send(buf->peek(),header_len);
            if(header_len > 0){
                buf->retrieve(header_len); // 消费掉已经转发完的头部
            }
            
            //非流式数据
            if(!parser->isResponseChunked())
            {    
                size_t n=std::min(buf->readableBytes(),parser->bytesRemaining());
                if(n>0){
                    client_conn_ptr->send(buf->peek(),n);
                    parser->consumeBody(buf,n);
                }
                if (parser->gotAll()) {
                    auto self=shared_from_this();
                    parser->reset();
                    if (returnCallback_) {
                        returnCallback_(self);
                    }
                }
            }
            else{
                std::string_view payload(buf->peek(), buf->readableBytes());
                bool finished = (payload.find("0\r\n\r\n") != std::string_view::npos);
                client_conn_ptr->send(buf);          // 把同包携带的chunk数据转发出去并消费
                if (finished){
                    LOG_INFO("Stream finished in first packet!");
                    auto self=shared_from_this();
                    parser->reset();
                    if (returnCallback_) returnCallback_(self);
                }
            }
        }
    }
    //说明客户端先断开连接了
    else
    {
        //此时这个RemoteClient不能归还连接池 因为其中可能残留很多上一个客户端的信息 必须立即销毁
        LOG_WARN("Client is gone prematurely, force closing dirty backend connection.");
        tc_->disconnect();
    }
    
}
void RemoteClient::sendToBackend(Buffer *buf,size_t n){
    if(!tc_->send(buf->peek(),n)){
        pendingBuffer_.append(buf->peek(),n);
    }
}
void RemoteClient::connect(){
    auto weakptr=weak_from_this();  
    tc_->setConnectionCallback([weakptr](const TcpConnectionPtr &conn){
        if(auto self=weakptr.lock()) 
            self->onConnection(conn);
    });
    tc_->setMessageCallback([weakptr](const TcpConnectionPtr &conn,
                                            Buffer* buf,
                                            Timestamp time)
    {
        if(auto self=weakptr.lock())
            self->onMessage(conn,buf,time);
    });
    tc_->connect();
}