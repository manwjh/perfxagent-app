#!/bin/bash

# WebSocket测试程序构建脚本
# 使用方法: ./build_test.sh [Debug|Release]

set -e

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

echo "=== WebSocket测试程序构建脚本 ==="
echo "脚本目录: $SCRIPT_DIR"
echo "项目根目录: $PROJECT_ROOT"

# 设置构建类型
BUILD_TYPE=${1:-Debug}
echo "构建类型: $BUILD_TYPE"

# 创建构建目录
BUILD_DIR="$SCRIPT_DIR/build"
echo "构建目录: $BUILD_DIR"

# 清理旧的构建文件
if [ -d "$BUILD_DIR" ]; then
    echo "清理旧的构建文件..."
    rm -rf "$BUILD_DIR"
fi

# 创建构建目录
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# 检查Qt6是否安装
echo "检查Qt6安装..."
if ! brew list qt@6 >/dev/null 2>&1; then
    echo "错误: Qt6未安装。请运行: brew install qt@6"
    exit 1
fi

# 检查必要的依赖
echo "检查依赖..."
QT6_PREFIX=$(brew --prefix qt@6)
echo "Qt6路径: $QT6_PREFIX"

# 配置CMake
echo "配置CMake..."
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
      -DCMAKE_PREFIX_PATH="$QT6_PREFIX" \
      -DQt6_DIR="$QT6_PREFIX/lib/cmake/Qt6" \
      "$SCRIPT_DIR"

# 编译
echo "编译测试程序..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 检查编译结果
if [ -f "bin/test_qt_websocket" ]; then
    echo "✅ 编译成功!"
    echo "可执行文件位置: $BUILD_DIR/bin/test_qt_websocket"
    
    # 询问是否运行测试
    echo ""
    read -p "是否运行测试程序? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "运行测试程序..."
        echo "注意: 如果需要设置ASR_TOKEN环境变量，请先运行: export ASR_TOKEN=your_token_here"
        echo ""
        cd bin
        ./test_qt_websocket
    fi
else
    echo "❌ 编译失败!"
    exit 1
fi

echo ""
echo "=== 构建完成 ===" 