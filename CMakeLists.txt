cmake_minimum_required(VERSION 3.10)
project(ChatServer)

# 启用 C 编译器（picohttpparser 等第三方 C 库需要）
enable_language(C)
set(CMAKE_C_STANDARD 99)

# C++ 标准设置
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g -pthread -fPIC") 
# 注意：编译动态库通常建议加上 -fPIC，确保代码在内存中的位置无关性

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_OBJECT_DIRECTORY ${CMAKE_BINARY_DIR}/obj)

# --- 新增逻辑：强制将库产出到项目根目录下的 lib 文件夹 ---
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 添加头文件搜索路径
include_directories(${CMAKE_SOURCE_DIR}/src)

# Add utils subdirectory
add_subdirectory(utils)

# 创建网络核心库：改为 SHARED (动态库)
add_library(Vortex-Net SHARED 
    src/InetAddress.cc 
    src/EventLoop.cc 
    src/Channel.cc 
    src/Poller.cc 
    src/EPollPoller.cc 
    src/DefaultPoller.cc
    src/socket.cc 
    src/Acceptor.cc 
    src/TcpServer.cc 
    src/TcpConnection.cc 
    src/EventLoopThread.cc 
    src/EventLoopThreadPool.cc 
    src/Timer.cc
    src/TimerQueue.cc
    src/Connector.cc
    src/TcpClient.cc
)

# 链接选项
target_link_libraries(Vortex-Net pthread utils)

# --- Proxy 动态库 ---
# 构建时模拟 Vortex-Net 头文件安装路径，使 Proxy 中的 <Vortex-Net/xxx.h> 能正确解析
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/Vortex-Net)
file(COPY ${CMAKE_SOURCE_DIR}/src/ DESTINATION ${CMAKE_BINARY_DIR}/include/Vortex-Net
     FILES_MATCHING PATTERN "*.h")
file(COPY ${CMAKE_SOURCE_DIR}/utils/ DESTINATION ${CMAKE_BINARY_DIR}/include/Vortex-Net
     FILES_MATCHING PATTERN "*.h")
# Proxy 头文件映射到 Vortex-Net/Proxy/ 子目录（安装后外部可用 <Vortex-Net/Proxy/xxx.h> 包含）
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include/Vortex-Net/Proxy)
file(COPY ${CMAKE_SOURCE_DIR}/Proxy/src/ DESTINATION ${CMAKE_BINARY_DIR}/include/Vortex-Net/Proxy
     FILES_MATCHING PATTERN "*.h")
include_directories(${CMAKE_BINARY_DIR}/include)

# Proxy 内部头文件路径
include_directories(${CMAKE_SOURCE_DIR}/Proxy/src)

# --- L4 Proxy（四层负载均衡）---
# 源文件：L4 专用组件 + HealthCheck（健康检查/节点选择）
add_library(L4_Proxy SHARED
    # HealthCheck 四层健康检查
    Proxy/src/L4/HealthCheck/CheckerTask.cc
    Proxy/src/L4/HealthCheck/HealthChecker.cc
    # Common 通用节点选择器
    Proxy/src/Common/Selector.cc
    # L4 四层负载均衡
    Proxy/src/L4/RemoteClient.cc
)

target_link_libraries(L4_Proxy pthread Vortex-Net)

set_target_properties(L4_Proxy PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Proxy/lib
)

# --- L7 Proxy（七层负载均衡）---
# 源文件：L7 专用组件（HTTP 解析 + 连接池 + 后端连接管理）
add_library(L7_Proxy SHARED
    # L7 七层负载均衡
    Proxy/src/L7/RemoteClient.cc
    Proxy/src/L7/HttpContext.cc
    Proxy/src/L7/ConnecitonPool.cc
    # Common 通用节点选择器
    Proxy/src/Common/Selector.cc
    # 第三方：HTTP 请求/响应解析器
    Proxy/src/L7/picohttpparser.c
)

target_link_libraries(L7_Proxy pthread Vortex-Net)

set_target_properties(L7_Proxy PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/Proxy/lib
)