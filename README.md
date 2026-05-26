# Vortex-Net: 高性能异步网络分发与代理框架

**Vortex-Net** 是一款基于 C++11 构建的高性能、轻量级异步网络底层框架。它专为高并发、低延迟的算力分发与中间件场景设计，集成了自研二进制协议栈与七层负载均衡核心逻辑。

## 核心特性
- **模型架构**：采用 Multiple Reactors + Thread Pool (One Loop Per Thread) 模型。主 Reactor 负责连接分发，子 Reactor 负责具体的 IO 事件处理。
- **IO 多路复用**：封装 Epoll 接口，默认采用 LT 触发模式（可扩展 ET），利用 IO 多路复用处理万级并发。
- **线程安全**：深度实践 RAII 机制，利用 `shared_from_this` 与 `weak_ptr` 解决了多线程环境下 TCP 连接对象生命周期管理的竞态条件问题。
- **内存管理**：实现自动增长的应用层 Buffer 缓冲区，利用 `readv` 结合栈空间缓存减少系统调用，优化堆内存分配。
- **定时器系统**：基于 `timerfd` 实现的高性能定时器，支持 $O(logN)$ 复杂度的超时连接清理。
- **异步日志**：(正在迁移) 具备双缓冲机制的异步日志系统，确保 IO 线程不被磁盘写入阻塞。

## 开发进度 (Roadmap)
- [x] 基于 Epoll 的 Reactor 核心事件循环
- [x] 多线程 EventLoopThreadPool 实现
- [x] 应用层 Buffer 动态缓冲区设计
- [x] 基于 `eventfd` 的异步唤醒机制
- [x] 基于 TLV (Type-Length-Value) 格式的二进制通信协议解析器**
- [x] 引入基于 `timerfd` 的高性能定时器（$O(\log N)$ 复杂度优化）
- [x] 高性能负载均衡分发引擎（数据传输层实现）
- [x] 负载均衡器的一致性哈希算法与健康检查模块
- [ ] **[计划中]** 集成 Protobuf 序列化支持
## 编译与构建
本项目采用 CMake 进行构建：
```bash
/bin/bash autobuild.sh

##项目说明与当前状态 (Current Status)

工业级高可用设计 (High Availability)：
已实现后端节点全生命周期管理。通过心跳检测机制解决了分布式系统中的“灰产”节点问题。
状态自愈：引入了状态机管理逻辑（Alive / Suspect / Dead），实现了节点的自动隔离与故障恢复后的自动权重恢复。
异步探测：探测逻辑与数据转发逻辑在不同 Reactor 线程中解耦，确保健康检查行为不会造成转发抖动。