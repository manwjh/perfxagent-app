//
// ASR 日志工具头文件
// 
// 本文件提供了统一的日志工具函数，包括时间戳功能
// 供 ASR 模块的各个组件使用
//

#ifndef ASR_LOG_UTILS_H
#define ASR_LOG_UTILS_H

#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>

namespace Asr {

// ============================================================================
// 日志工具函数
// ============================================================================

/**
 * @brief 获取当前时间戳，精确到毫秒
 * @return 格式化的时间戳字符串 (HH:MM:SS.mmm)
 */
inline std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_val), "%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

/**
 * @brief 带时间戳的普通日志输出
 * @param message 日志消息
 */
inline void logWithTimestamp(const std::string& message) {
    std::string timestamp = getCurrentTimestamp();
    std::cout << "[" << timestamp << "] " << message << std::endl;
}

/**
 * @brief 带时间戳的错误日志输出
 * @param message 错误消息
 */
inline void logErrorWithTimestamp(const std::string& message) {
    std::string timestamp = getCurrentTimestamp();
    std::cerr << "[" << timestamp << "] ❌ " << message << std::endl;
}

/**
 * @brief 将字节数组转换为十六进制字符串
 * @param data 字节数组
 * @return 十六进制字符串
 */
inline std::string hexString(const std::vector<uint8_t>& data) {
    std::stringstream ss;
    for (size_t i = 0; i < data.size(); ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        if (i < data.size() - 1) ss << " ";
    }
    return ss.str();
}

} // namespace Asr

#endif // ASR_LOG_UTILS_H 