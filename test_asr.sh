#!/bin/bash

# ASR Qt客户端测试脚本
# 用于构建和测试ASR模块

set -e  # 遇到错误时退出

echo "🚀 ASR Qt客户端测试脚本"
echo "========================"

# 检查是否在正确的目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ 错误: 请在项目根目录运行此脚本"
    exit 1
fi

# 创建构建目录
echo "📁 创建构建目录..."
mkdir -p build
cd build

# 配置CMake
echo "⚙️  配置CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 构建项目
echo "🔨 构建项目..."
make -j$(nproc)

# 检查构建结果
if [ ! -f "bin/asr_usage_example" ]; then
    echo "❌ 构建失败: 未找到 asr_usage_example 可执行文件"
    exit 1
fi

echo "✅ 构建成功!"

# 检查音频文件
if [ ! -f "../sample/38s.wav" ]; then
    echo "⚠️  警告: 未找到测试音频文件 ../sample/38s.wav"
    echo "   请确保音频文件存在，或者修改测试代码中的文件路径"
else
    echo "✅ 找到测试音频文件: ../sample/38s.wav"
fi

# 设置环境变量（可选）
echo "🔧 设置环境变量..."
export ASR_APP_ID="8388344882"
export ASR_ACCESS_TOKEN="vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
export ASR_SECRET_KEY="oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e"

echo "📋 环境变量已设置:"
echo "   ASR_APP_ID: $ASR_APP_ID"
echo "   ASR_ACCESS_TOKEN: ${ASR_ACCESS_TOKEN:0:8}****"
echo "   ASR_SECRET_KEY: ${ASR_SECRET_KEY:0:8}****"

# 运行测试
echo ""
echo "🧪 开始运行ASR测试..."
echo "================================"

# 切换到bin目录
cd bin

# 运行基本测试
echo "📋 运行基本连接测试..."
./asr_qt_test

echo ""
echo "📋 运行完整功能测试..."
./asr_usage_example

echo ""
echo "🎉 测试完成!"
echo "================================"
echo "📊 测试结果:"
echo "   - 基本测试: 完成"
echo "   - 完整测试: 完成"
echo ""
echo "💡 提示:"
echo "   - 如果遇到连接错误，请检查网络连接"
echo "   - 如果遇到认证错误，请检查环境变量设置"
echo "   - 如果遇到音频文件错误，请检查文件路径" 