#!/bin/bash

echo "=== ASR 协议日志控制演示 ==="
echo

# 检查当前配置
echo "1. 检查当前 debug 配置..."
ACTUAL_CONFIG=$(sed -n '36p' include/asr/asr_debug_config.h)
if [ "$ACTUAL_CONFIG" = "#define ASR_DEBUG_MODE" ]; then
    echo "   🔍 当前配置：协议日志已开启（开发环境）"
    PROTOCOL_LOG_ENABLED=true
elif [ "$ACTUAL_CONFIG" = "// #define ASR_DEBUG_MODE" ]; then
    echo "   ✅ 当前配置：协议日志已关闭（生产环境）"
    PROTOCOL_LOG_ENABLED=false
else
    echo "   ❓ 未知配置状态: '$ACTUAL_CONFIG'"
    PROTOCOL_LOG_ENABLED=false
fi

echo

# 编译项目
echo "2. 编译项目..."
cd build
make > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "   ✅ 编译成功"
else
    echo "   ❌ 编译失败"
    exit 1
fi

echo

# 运行测试（如果有环境变量）
echo "3. 运行测试..."
cd ..
if [ -f "examples/asr_config_example.env" ]; then
    echo "   📝 发现配置文件，可以运行测试"
    echo "   💡 要看到协议日志效果，请："
    echo "      1. 设置真实的 ASR 凭据"
    echo "      2. 运行: ./build/bin/asr_simple_example"
    echo
    echo "   🔧 控制协议日志的方法："
    echo "      - 关闭协议日志：注释掉 include/asr/asr_debug_config.h 中的 #define ASR_DEBUG_MODE"
    echo "      - 开启协议日志：取消注释 include/asr/asr_debug_config.h 中的 #define ASR_DEBUG_MODE"
    echo "      - 重新编译：make clean && make"
else
    echo "   ⚠️  未找到配置文件"
fi

echo

# 显示配置说明
echo "4. 配置说明："
echo "   📁 配置文件：include/asr/asr_debug_config.h"
echo "   🔧 主要开关：ASR_DEBUG_MODE"
echo "   📊 协议日志：ASR_ENABLE_PROTOCOL_LOG"
echo
echo "   📋 当前所有调试开关："
grep -A 10 "调试开关控制" include/asr/asr_debug_config.h | grep "ASR_ENABLE" | sed 's/^/      /'

echo
echo "=== 演示完成 ===" 