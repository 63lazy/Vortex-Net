#include <Vortex-Net/TcpClient.h>
#include <Vortex-Net/EventLoop.h>
#include <Vortex-Net/Buffer.h>
#include <Vortex-Net/Timestamp.h>
#include <Vortex-Net/logger.h>
#include <iostream>
#include <string>
class echo_Client{
public:
    echo_Client(EventLoop *loop,InetAddress &serverAddr,std::string &name):
                tc_(std::make_shared<TcpClient>(loop,serverAddr,name)),
                loop_(loop)
    {
        tc_->setConnectionCallback([this](const TcpConnectionPtr &conn){
            onConnection(conn);
        });
        tc_->setMessageCallback([this](const TcpConnectionPtr &conn,Buffer *buf,Timestamp Time){
            onMessage(conn,buf,Time);
        });
    }
    void connect(){
        tc_->connect();
    }
    void disconnect(){
        tc_->disconnect();
    }
    
private:
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected())
        {
            LOG_INFO("connection UP :%s",conn->peerAddress().toIpPort().c_str());
            conn->send("hello");
        }
        else
        {
            LOG_INFO("connection DOWN :%s",conn->peerAddress().toIpPort().c_str());
        }
    }
    void onMessage(const TcpConnectionPtr &conn,Buffer *buf,Timestamp Time){
        std::string msg=buf->retrieveAllAsString();
        std::cout<<msg<<std::endl;
        if(msg=="hello")
            tc_->disconnect();
    }
    //tc_必须由shared_ptr管理
    //因为TcpClient的函数使用了shared_from_this()
    std::shared_ptr<TcpClient> tc_;
    EventLoop *loop_;
    
};

int main(){
    EventLoop loop;
    InetAddress serverAddr(8000,"127.0.0.1");
    std::string name="client_1";
    echo_Client client(&loop,serverAddr,name);
    client.connect();

    loop.loop();
    
    return 0;
}