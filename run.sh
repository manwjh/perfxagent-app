#!/bin/bash

# PerfxAgent-ASR 启动脚本
echo "🚀 启动 PerfxAgent-ASR..."

# 检查程序是否存在
if [ ! -f "./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR" ]; then
    echo "❌ 程序不存在，请先编译项目："
    echo "   ./scripts/build_dev.sh"
    exit 1
fi

# 启动程序
echo "✅ 程序启动中..."
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR

echo "👋 程序已退出" 