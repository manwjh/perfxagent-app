//
// ASR 调试配置头文件
// 
// 本文件定义了 ASR 模块的调试配置选项
// 通过修改此文件来控制调试行为，无需修改上层应用代码
// 
// 使用方法：
// 1. 在开发时取消注释需要的调试选项
// 2. 在发布时注释掉所有调试选项
// 3. 重新编译项目
//

#ifndef ASR_DEBUG_CONFIG_H
#define ASR_DEBUG_CONFIG_H

// ============================================================================
// 日志级别枚举 - 统一使用此枚举
// ============================================================================

enum AsrLogLevel {
    ASR_LOG_NONE = 0,      // 无日志
    ASR_LOG_ERROR = 1,     // 仅错误
    ASR_LOG_WARN = 2,      // 警告和错误
    ASR_LOG_INFO = 3,      // 信息、警告和错误
    ASR_LOG_DEBUG = 4,     // 调试信息（包含所有）
    ASR_LOG_VERBOSE = 5    // 详细调试（包含协议细节）
};

// ============================================================================
// 调试配置 - 编译时控制
// ============================================================================

// 主调试模式开关
// 启用后：日志级别设为DEBUG，启用所有调试开关
// 禁用时：日志级别设为INFO，禁用所有调试开关
// #define ASR_DEBUG_MODE

// 日志级别控制
#ifdef ASR_DEBUG_MODE
    #define ASR_LOG_LEVEL ASR_LOG_DEBUG
#else
    #define ASR_LOG_LEVEL ASR_LOG_INFO
#endif

// 调试开关控制
#ifdef ASR_DEBUG_MODE
    #define ASR_ENABLE_BUSINESS_LOG 1    // 业务逻辑日志
    #define ASR_ENABLE_FLOW_LOG 1        // 流程日志
    #define ASR_ENABLE_DATA_LOG 1        // 数据日志
    #define ASR_ENABLE_PROTOCOL_LOG 1    // 协议日志
    #define ASR_ENABLE_AUDIO_LOG 1       // 音频日志
#else
    #define ASR_ENABLE_BUSINESS_LOG 0
    #define ASR_ENABLE_FLOW_LOG 0
    #define ASR_ENABLE_DATA_LOG 0
    #define ASR_ENABLE_PROTOCOL_LOG 0
    #define ASR_ENABLE_AUDIO_LOG 0
#endif

// ============================================================================
// 使用说明
// ============================================================================

/*
调试配置使用指南：

1. 生产环境（默认）
   - 不启用 ASR_DEBUG_MODE
   - 日志级别：INFO
   - 所有调试开关：关闭
   - 只输出重要信息

2. 开发环境
   - 启用 ASR_DEBUG_MODE
   - 日志级别：DEBUG
   - 所有调试开关：开启
   - 输出详细调试信息

3. 自定义配置（高级用法）
   - 手动设置 ASR_LOG_LEVEL
   - 手动控制各个调试开关
   - 例如：只启用业务日志和流程日志

配置示例：

// 生产环境（默认）
// #define ASR_DEBUG_MODE

// 开发环境
#define ASR_DEBUG_MODE

// 自定义配置（高级用法）
// #define ASR_DEBUG_MODE
// #define ASR_LOG_LEVEL ASR_LOG_DEBUG
// #define ASR_ENABLE_BUSINESS_LOG 1
// #define ASR_ENABLE_FLOW_LOG 1
// #define ASR_ENABLE_DATA_LOG 0
// #define ASR_ENABLE_PROTOCOL_LOG 0
// #define ASR_ENABLE_AUDIO_LOG 0
*/

#endif // ASR_DEBUG_CONFIG_H 