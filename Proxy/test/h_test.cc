#include <Vortex-Net/Proxy/L4/LB_Server.h>
#include <Vortex-Net/Proxy/L4/HealthCheck/HealthChecker.h>
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
        std::make_shared<ServerNode>(InetAddress(8002), 1),
        //std::make_shared<ServerNode>(InetAddress(8003), 1)
    };

    HealthChecker checker(&loop);

    for (auto& node : serverGroup) {
        checker.addNode(node);
    }
    checker.start();
    loop.loop();
}