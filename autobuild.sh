#!/bin/bash

set -e

# 1. 编译
if [ ! -d "build" ]; then
    mkdir build
fi
rm -rf $(pwd)/build/*
cd build &&
    cmake .. &&
    make
cd ..

# 2. 重新创建干净的安装目录 (平铺化)r5
sudo rm -rf /usr/include/Vortex-Net
sudo mkdir -p /usr/include/Vortex-Net

# 3. 使用 find 强制搜寻并拷贝所有头文件到根目录
# 把 src/ 和 utils/ 下的所有 .h 统统拷到 /usr/include/Vortex-Net/ 这一层
sudo find src -name "*.h" -exec cp {} /usr/include/Vortex-Net/ \;
sudo find utils -name "*.h" -exec cp {} /usr/include/Vortex-Net/ \;

# 4. 拷贝 Proxy 头文件到独立子目录
sudo mkdir -p /usr/include/Vortex-Net/Proxy
sudo find Proxy/src -name "*.h" -exec cp {} /usr/include/Vortex-Net/Proxy/ \;

# 5. 拷贝动态库并刷新
sudo cp $(pwd)/lib/libVortex-Net.so /usr/lib
sudo cp $(pwd)/Proxy/lib/libProxy.so /usr/lib
sudo ldconfig

echo "Build and Flattened Installation Success!"