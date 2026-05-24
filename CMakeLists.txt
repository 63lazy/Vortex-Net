cmake_minimum_required(VERSION 3.10)
project(ChatServer)

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