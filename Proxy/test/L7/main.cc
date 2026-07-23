#include <Vortex-Net/Proxy/L7/LB_Server.h>
#include <Vortex-Net/EventLoop.h>
#include <Vortex-Net/InetAddress.h>
#include <Vortex-Net/Proxy/Common/ServerNode.h>

#include <memory>
#include <vector>

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    std::vector<std::shared_ptr<ServerNode>> serverGroup{
        std::make_shared<ServerNode>(InetAddress(8001), 5),
        //std::make_shared<ServerNode>(InetAddress(8002), 1),
        std::make_shared<ServerNode>(InetAddress(8003), 1)
    };
    
    LB_Server server(&loop, addr, "LB_server", serverGroup);
    server.addNode("qwen2.5:1.5b",std::make_shared<ServerNode>(InetAddress(11434), 1));
    server.addNode("deepseek-r1:1.5b",std::make_shared<ServerNode>(InetAddress(11435), 1));

    server.start();

    loop.loop();
}