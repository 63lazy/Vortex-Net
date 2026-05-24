#include <Vortex-Net/TcpServer.h>
#include <Vortex-Net/logger.h>
#include <string>
#include "../Codec/TLVCodec.h"
class TLVServer{
public:
    TLVServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg):
              server_(loop,listenAddr,nameArg),
              loop_(loop)
    {
        //设置回调函数
        server_.setConnectionCallback([this](const TcpConnectionPtr &conn){
            onConnection(conn);
        });
        server_.setMessageCallback([this](const TcpConnectionPtr &conn,
                                            Buffer* buf, 
                                            Timestamp time)
        {
            TLVCodec::onTLVmessage(conn,buf,time);
        });
        //设置线程数
        server_.setThreadNum(1);
    }

    void start(){
        server_.start();
    }
private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if(conn->connected()){
            LOG_INFO("connection UP :%s",conn->peerAddress().toIpPort().c_str());
        }
        else{
            LOG_INFO("connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        }
    }


    EventLoop *loop_;
    TcpServer server_;

    
};
int main(){
    EventLoop loop;
    InetAddress addr(8000,"0.0.0.0");
    TLVServer server(&loop,addr,"server_2");
    server.start();
    loop.loop();

    return 0;
}
