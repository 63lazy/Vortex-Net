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

# 2. 重新创建干净的安装目录
sudo rm -rf /usr/include/Vortex-Net
sudo mkdir -p /usr/include/Vortex-Net

# 3. 拷贝 Vortex-Net 核心头文件（平铺化）
sudo find src -name "*.h" -exec cp {} /usr/include/Vortex-Net/ \;
sudo find utils -name "*.h" -exec cp {} /usr/include/Vortex-Net/ \;

# 4. 拷贝 Proxy 头文件（保留目录结构：Common/ L4/ L7/）
sudo mkdir -p /usr/include/Vortex-Net/Proxy
sudo rsync -av --include='*/' --include='*.h' --exclude='*' Proxy/src/ /usr/include/Vortex-Net/Proxy/

# 5. 拷贝动态库并刷新
sudo cp $(pwd)/lib/libVortex-Net.so /usr/lib
sudo cp $(pwd)/Proxy/lib/libL4_Proxy.so /usr/lib
sudo cp $(pwd)/Proxy/lib/libL7_Proxy.so /usr/lib
sudo ldconfig

echo "Build and Flattened Installation Success!"