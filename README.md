# Vortex-Net: 高性能 LLM API 网关与通用负载均衡框架

**Vortex-Net** 是一款基于 C++20 构建的高性能、生产级网络代理框架。项目以 **大模型 API 网关 (LLM Gateway)** 为核心场景，同时提供通用的四层/七层负载均衡能力。整体采用 Multiple Reactors + Thread Pool 架构，从 Epoll 事件循环、应用层协议解析到后端连接池管理全部自研，不依赖任何第三方网络库。

---

## 架构概览

```
                        ┌──────────────────────────────────┐
                        │         Vortex-Net Core          │
                        │  ┌────────────────────────────┐  │
                        │  │  EventLoop + ThreadPool    │  │
                        │  │  (One Loop Per Thread)     │  │
                        │  │  Epoll LT + eventfd        │  │
                        │  │  timerfd (O(log N))        │  │
                        │  │  Buffer (readv scatter)    │  │
                        │  │  TLV Binary Codec          │  │
                        │  └────────────────────────────┘  │
                        └──────────┬───────────────────────┘
                                   │
              ┌────────────────────┼────────────────────┐
              │                                        │
    ┌─────────▼──────────┐              ┌──────────────▼─────────┐
    │   L4 Proxy (TCP)   │              │   L7 Proxy (HTTP)      │
    │  ┌──────────────┐  │              │  ┌──────────────────┐  │
    │  │ 字节流转发    │  │              │  │ HTTP 协议解析    │  │
    │  │ 一致性哈希    │  │              │  │ 模型路由分发    │  │
    │  │ 加权轮询      │  │              │  │ Connection Pool  │  │
    │  │ 健康检查      │  │              │  │ Chunked 流式传输 │  │
    │  │ 状态机自愈    │  │              │  │ Keep-Alive 复用  │  │
    │  └──────────────┘  │              │  └──────────────────┘  │
    └────────────────────┘              └─────────────────────────┘
```

---

## 核心特性

### 网络底层框架 (Vortex-Net Core)

| 模块 | 描述 |
|------|------|
| **Reactor 模型** | Multiple Reactors + One Loop Per Thread。主 Reactor 负责 `accept` 分发，子 Reactor 负责 IO 读写与业务处理，线程数与 CPU 核心数自适应匹配 |
| **IO 多路复用** | 封装 Epoll，默认 LT 触发模式（可扩展 ET），单机支持万级并发连接 |
| **线程安全** | 深度实践 RAII，利用 `shared_from_this` + `weak_ptr` 解决多线程下 TCP 连接对象的生命周期竞态条件，杜绝悬空指针 |
| **内存管理** | 自研自动增长的应用层 Buffer，结合 `readv` 栈空间缓存实现 scatter-gather IO，减少 `read` 系统调用次数，降低堆内存碎片 |
| **定时器系统** | 基于 `timerfd` 的高性能定时器，`std::multiset` 组织为红黑树，插入/删除/查找均为 $O(\log N)$ |
| **异步唤醒** | 基于 `eventfd` 的跨线程唤醒机制，实现线程间任务安全派发 |
| **二进制协议** | 自研 TLV (Type-Length-Value) 格式通信协议，支持自定义消息类型的编解码 |
| **异步连接器** | 非阻塞 TCP 连接器，内置自动重连与指数退避 |

### 四层负载均衡 (L4 Proxy)

```
Client ──TCP──▶ L4 LB Server ──TCP──▶ Backend Pool
                    │                    │
                    │    ┌───────────┐   │
                    └───▶│ Selector  │◀──┘
                         │ (哈希环)  │
                         └───────────┘
                         ┌───────────┐
                         │HealthCheck│
                         │ Alive/Dead│
                         └───────────┘
```

- **透明转发**：在 TCP 层进行逐字节双向转发，对上层协议完全透明
- **一致性哈希**：每个物理节点映射 100 个虚拟节点到哈希环，节点上下线时仅影响局部数据分布，最小化连接中断
- **加权轮询 (SWRR)**：平滑加权轮询算法，权重越高的节点获得越多的请求分配，避免流量尖刺
- **健康检查与状态自愈**：
  - 异步 TCP 端口探测，与数据转发在不同的 Reactor 线程中解耦，探测行为不会造成转发抖动
  - 状态机管理：`Alive → Suspect → Dead`，连续 N 次失败才标记宕机，避免网络瞬时抖动误判
  - 故障节点自动隔离，恢复后自动权重恢复
- **写时复制 (COW)**：节点选择时采用 `shared_ptr` 写时复制替代加锁，读取路径零锁开销

### 七层负载均衡 (L7 Proxy / LLM Gateway)

```
                         ┌───────────────┐
                         │  L7 LB Server │
                         │  (HTTP 网关)  │
                         └───┬───┬───┬───┘
                             │   │   │
                    ┌────────┼───┼───┼────────┐
                    │        │   │   │        │
                    ▼        ▼   │   ▼        ▼
              /qwen/...  /llama/...  │  /gpt/...  /echo/...
                    │        │       │   │        │
                    ▼        ▼       ▼   ▼        ▼
              ┌─────────────────────────────────────────┐
              │           Backend Model Clusters         │
              │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐   │
              │  │ Qwen │ │Llama │ │ GPT  │ │Echo  │   │
              │  │Pool  │ │Pool  │ │Pool  │ │Pool  │   │
              │  └──────┘ └──────┘ └──────┘ └──────┘   │
              └─────────────────────────────────────────┘
```

- **HTTP 协议深度解析**：集成 picohttpparser（零拷贝 C 解析器），支持完整的 HTTP 请求/响应解析
- **模型感知路由 (Model-Aware Routing)**：根据请求路径中的模型名称（如 `/v1/chat/completions` body 中的 `model` 字段）将请求动态路由到对应的后端模型集群，支持 Qwen、Llama、GPT 等多模型混合部署
- **动态路由表**：支持运行时按模型名注册/注销后端节点集群，未命中时回退到默认兜底集群
- **连接池管理**：
  - 每线程独立的连接池 (`thread_local`)，实现无锁的连接借用/归还
  - 每后端节点最大空闲连接数可配置，超限连接自动关闭
  - 连接忙闲状态追踪，支持 Keep-Alive 长连接复用
- **流式传输支持**：完整实现 HTTP Chunked Transfer Encoding 解码，支持 SSE (Server-Sent Events) 和大模型流式推理输出
- **报文边界感知**：精确区分 HTTP 头部与 Body 的边界，支持头部与 Body 同包到达、跨包 Body 等复杂场景，保证流式场景下不丢数据、不粘包
- **请求/响应生命周期管理**：`HttpContext` 状态机追踪完整请求-响应生命周期 (`kExpectHeaders → kExpectBody → kGotAll`)，支持连接复用场景下的状态重置

---

## 性能设计亮点

| 特性 | 实现方式 | 收益 |
|------|----------|------|
| 零锁读取 | 节点选择采用 COW (`shared_ptr` 原子交换) | 读取路径完全无锁，避免缓存行争用 |
| 线程局部连接池 | `thread_local` 存储，无共享状态 | 连接借还零竞争，延迟稳定 |
| 栈上解析 | `HttpRequest` 对短字段启用 SBO (Small Buffer Optimization) | 短字符串零堆分配 |
| Scatter-Gather IO | Buffer + `readv` + 栈空间扩展 | 减少 `read` 系统调用，降低 CPU 开销 |
| 零拷贝解析 | picohttpparser 原地解析 HTTP 头部 | 不额外分配内存，解析速度 > 50K req/s |
| 异步健康检查 | 探测线程与转发线程隔离 | 健康检查零干扰，转发延迟稳定 |

---

## 项目结构

```
Vortex-Net/
├── src/                    # 网络核心库 (Vortex-Net Core)
│   ├── EventLoop.cc/.h      # 事件循环 (Reactor)
│   ├── EPollPoller.cc/.h    # Epoll 封装
│   ├── TcpServer.cc/.h      # TCP 服务器
│   ├── TcpClient.cc/.h      # TCP 客户端 (异步连接器)
│   ├── TcpConnection.cc/.h  # TCP 连接对象
│   ├── Acceptor.cc/.h       # 连接接收器
│   ├── Connector.cc/.h      # 异步连接器
│   ├── Channel.cc/.h        # 文件描述符事件分发
│   ├── TimerQueue.cc/.h     # 定时器队列 (timerfd)
│   ├── EventLoopThreadPool  # 线程池
│   └── ...
├── Proxy/                  # 负载均衡代理层
│   └── src/
│       ├── Common/          # 通用组件
│       │   ├── Selector     # 负载均衡算法 (哈希环 + SWRR)
│       │   └── ServerNode   # 后端节点状态管理
│       ├── L4/              # 四层代理
│       │   ├── LB_Server    # L4 负载均衡服务器
│       │   ├── RemoteClient # 后端 TCP 连接管理
│       │   └── HealthCheck/ # 健康检查模块
│       └── L7/              # 七层代理 (LLM Gateway)
│           ├── LB_Server    # L7 负载均衡服务器 (HTTP 网关)
│           ├── HttpContext  # HTTP 协议解析 & 状态机
│           ├── RemoteClient # 后端 HTTP 连接 & 双向转发
│           ├── ConnectionPool # 连接池 (thread_local)
│           └── picohttpparser # 高性能 HTTP 解析器 (C)
├── Codec/                  # 协议编解码
│   ├── TLVCodec.h           # TLV 二进制协议
│   └── protocol.h           # 消息格式定义
├── utils/                  # 工具库
│   ├── Buffer               # 应用层动态缓冲区
│   ├── logger               # 异步日志
│   ├── Timestamp            # 时间戳工具
│   └── thread               # 线程封装
├── CMakeLists.txt           # 构建配置
└── lib/                     # 编译产物
```

---

## 编译与构建

```bash
# 一键构建
/bin/bash autobuild.sh

# 或手动构建
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

编译产出：
- `lib/libVortex-Net.so` — 网络核心库
- `Proxy/lib/libL4_Proxy.so` — 四层负载均衡库
- `Proxy/lib/libL7_Proxy.so` — 七层负载均衡库 (LLM Gateway)

---

## 技术栈与工程实践

- **语言标准**：C++20 (Core) + C99 (picohttpparser)
- **构建系统**：CMake 3.10+，动态库产出，支持模块化集成
- **并发模型**：Multiple Reactors + One Loop Per Thread
- **内存模型**：RAII + `shared_ptr`/`weak_ptr` 生命周期管理 + SBO 优化
- **IO 模型**：Non-blocking IO + Epoll LT + eventfd 异步唤醒
- **设计模式**：Reactor、Observer/Callback、Strategy (Selector 算法族)、Object Pool (ConnectionPool)、Copy-on-Write

---

## Roadmap

- [x] Epoll Reactor 核心事件循环
- [x] 多线程 EventLoopThreadPool
- [x] 应用层 Buffer 动态缓冲区 (readv scatter-gather)
- [x] eventfd 异步跨线程唤醒
- [x] timerfd 高性能定时器 ($O(\log N)$)
- [x] TLV 二进制通信协议编解码
- [x] 异步 TCP 连接器 (Connector + TcpClient)
- [x] **四层负载均衡**：一致性哈希 + 加权轮询 + 健康检查 + 状态自愈
- [x] **七层负载均衡 (LLM Gateway)**：HTTP 解析 + 模型路由 + 连接池 + Chunked 流式传输
- [ ] 配置文件驱动 (YAML/JSON) 替代硬编码
- [ ] 管理 API 端点 (metrics、health 查询、动态上下线)
- [ ] 限流与熔断 (Rate Limiting & Circuit Breaker)
- [ ] Prometheus metrics 集成
- [ ] WebSocket 协议升级支持
- [ ] gRPC 协议支持
- [ ] 基于令牌的认证与鉴权中间件
- [ ] 请求级缓存 (语义缓存，适用于 LLM 场景)
