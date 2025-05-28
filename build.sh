#!/bin/bash

# 检查必要的命令
command -v cmake >/dev/null 2>&1 || { echo "需要安装 cmake"; exit 1; }
command -v make >/dev/null 2>&1 || { echo "需要安装 make"; exit 1; }

# 获取 CPU 核心数
if [ "$(uname)" == "Darwin" ]; then
    # macOS
    CORES=$(sysctl -n hw.ncpu)
else
    # Linux
    CORES=$(nproc)
fi

# 检查依赖
if [ "$(uname)" == "Darwin" ]; then
    # macOS
    if ! brew list portaudio >/dev/null 2>&1; then
        echo "正在安装 portaudio..."
        brew install portaudio
    fi
    if ! brew list qt@6 >/dev/null 2>&1; then
        echo "正在安装 qt@6..."
        brew install qt@6
    fi
fi

# 创建构建目录
echo "创建构建目录..."
mkdir -p build
cd build || exit 1

# 配置项目
echo "配置项目..."
cmake .. || { echo "CMake 配置失败"; exit 1; }

# 构建项目
echo "构建项目..."
make -j${CORES} || { echo "构建失败"; exit 1; }

# 检查示例程序是否存在
if [ ! -f "bin/audio_example" ]; then
    echo "错误：示例程序未生成"
    exit 1
fi

# 运行示例程序
echo "运行示例程序..."
./bin/audio_example 