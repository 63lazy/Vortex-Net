#pragma once
#include <Vortex-Net/Proxy/L7/Middleware/ApiKeyAuth.h>
#include <Vortex-Net/Proxy/L7/Middleware/Middleware.h>

class AuthMiddleware : public HttpMiddleware {
    ApiKeyAuthenticator& auth_;
public:
    explicit AuthMiddleware(ApiKeyAuthenticator& auth) : auth_(auth) {}

    MiddlewareAction process(const TcpConnectionPtr& conn,
                             const std::shared_ptr<HttpContext>& parser,
                             const std::shared_ptr<ClusterManager>& cluster) override {
        (void)cluster; // 鉴权不依赖集群

        // 只有 /v1 路径需要鉴权，普通请求直接跳过
        if (!parser->request().path.starts_with("/v1")) {
            return MiddlewareAction::Continue;
        }
        if (!auth_.authenticate(parser->request().headers)) {
            const auto resp = ApiKeyAuthenticator::unauthorizedResponse();
            conn->send(resp.data(), resp.size());
            conn->shutdown();
            return MiddlewareAction::Intercept; // 拦截
        }
        return MiddlewareAction::Continue; // 放行
    }
};