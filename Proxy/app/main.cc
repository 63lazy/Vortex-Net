#include "LB_Server.h"
#include <thread>
int main(){
    EventLoop loop;
    InetAddress addr(8000);
    std::vector<std::shared_ptr<ServerNode>> serverGroup{
        std::make_shared<ServerNode>(InetAddress(8001), 5),
        std::make_shared<ServerNode>(InetAddress(8002), 1),
        std::make_shared<ServerNode>(InetAddress(8003), 1)
    };

    std::thread checker_thread([serverGroup](){
        EventLoop loop_h;
        HealthChecker checker(&loop_h);

        for(auto& node : serverGroup) {
            checker.addNode(node); 
        }
        checker.start();
        loop_h.loop();
    });

    LB_Server server(&loop,addr,"LB_server",serverGroup);
    server.start();

    loop.loop();
    
    checker_thread.join();
};