#include <Vortex-Net/TcpServer.h>
#include <Vortex-Net/logger.h>
#include <string>

class EchoServer{
public:
    EchoServer(EventLoop *loop,
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
            onMessage(conn,buf,time);
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
            std::weak_ptr<TcpConnection> weak_conn(conn);
            loop_->runEvery(10.0,[this,weak_conn](){
                kickIdleConnection(weak_conn);
            });
        }
        else{
            LOG_INFO("connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        }
    }
    void kickIdleConnection(std::weak_ptr<TcpConnection> weakConn) {
        auto conn = weakConn.lock(); // 尝试提升
        if (conn) {
            int64_t now = Timestamp::now().microSecondsSinceEpoch();
            int64_t lastActive = conn->lastReceiveTime(); // handleRead 里更新的变量
            
            if (now - lastActive > 10 * 1000 * 1000) { // 真正超过 10s 没动静
                LOG_INFO("Idle timeout, kick connection: %s", conn->peerAddress().toIpPort().c_str());
                conn->shutdown();
            }
            // 如果没超时，由于是 runEvery，10 秒后它会自动再次进来检查
        }
    }
    void onMessage(const TcpConnectionPtr &conn,
                   Buffer* buf, 
                   Timestamp time)
    {
        std::string msg=buf->retrieveAllAsString();
        LOG_INFO("Received %lu bytes from %s: %s", 
             msg.size(), 
             conn->peerAddress().toIpPort().c_str(),
             msg.c_str());
        conn->send(msg);
        //conn->shutdown();
    }

    EventLoop *loop_;
    TcpServer server_;

};
int main(){
    EventLoop loop;
    InetAddress addr(8000,"0.0.0.0");
    EchoServer server(&loop,addr,"server_1");
    server.start();
    loop.loop();

    return 0;
}
