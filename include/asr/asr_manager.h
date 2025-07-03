//
// ASR 管理模块头文件
// 
// 本文件定义了 ASR 管理模块，提供统一的 ASR 接口
// 支持多种 ASR 客户端实现（IXWebSocket、Qt、WebSocketpp 等）
// 
// 主要功能：
// - 统一的 ASR 接口
// - 多种客户端实现的管理
// - 运行时客户端选择
// - 配置管理和凭据管理
// - 连接时间统计和持久化
// 
// 作者: PerfXAgent Team
// 版本: 1.6.0
// 日期: 2024
// 参考: 火山引擎 ASR WebSocket 协议文档
//

#pragma once

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include "asr/asr_client.h"
#include "asr/asr_debug_config.h"
#include "secure_key_manager.h"     //仅服务于LOG打印信息的隐码

namespace Asr {

// ============================================================================
// 枚举定义
// ============================================================================

/**
 * @brief ASR 客户端类型枚举
 * 当前项目仅支持IXWebSocket实现
 */
enum class ClientType {
    IXWEBSOCKET = 1    // 基于 IXWebSocket 的实现
};

/**
 * @brief ASR 状态枚举
 */
enum class AsrStatus {
    DISCONNECTED = 0,   // 未连接
    CONNECTING = 1,     // 连接中
    CONNECTED = 2,      // 已连接
    RECOGNIZING = 3,    // 识别中
    ERROR = 4           // 错误状态
};

// ============================================================================
// 计时器相关结构体定义
// ============================================================================

/**
 * @brief 单次连接会话统计
 */
struct ConnectionSession {
    std::chrono::system_clock::time_point connectTime;    // 连接开始时间
    std::chrono::system_clock::time_point disconnectTime; // 连接结束时间
    std::chrono::milliseconds duration;                   // 连接持续时间
    std::string sessionId;                                // 会话ID
    bool isCompleted;                                     // 是否正常完成
    
    ConnectionSession() : duration(0), isCompleted(false) {}
    
    // 计算持续时间
    void calculateDuration() {
        if (connectTime.time_since_epoch().count() > 0 && 
            disconnectTime.time_since_epoch().count() > 0) {
            duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                disconnectTime - connectTime);
        }
    }
    
    // 获取格式化的持续时间字符串
    std::string getFormattedDuration() const {
        auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
        auto hours = totalSeconds / 3600;
        auto minutes = (totalSeconds % 3600) / 60;
        auto seconds = totalSeconds % 60;
        
        std::stringstream ss;
        if (hours > 0) {
            ss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            ss << minutes << "m " << seconds << "s";
        } else {
            ss << seconds << "s";
        }
        return ss.str();
    }
};

/**
 * @brief 每日使用统计
 */
struct DailyUsageStats {
    std::string date;                                     // 日期 (YYYY-MM-DD)
    std::chrono::milliseconds totalDuration;              // 当日总使用时长
    int sessionCount;                                     // 会话次数
    std::vector<ConnectionSession> sessions;              // 当日所有会话
    
    DailyUsageStats() : totalDuration(0), sessionCount(0) {}
    
    // 添加会话
    void addSession(const ConnectionSession& session) {
        sessions.push_back(session);
        totalDuration += session.duration;
        sessionCount++;
    }
    
    // 获取格式化的总时长字符串
    std::string getFormattedTotalDuration() const {
        auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(totalDuration).count();
        auto hours = totalSeconds / 3600;
        auto minutes = (totalSeconds % 3600) / 60;
        auto seconds = totalSeconds % 60;
        
        std::stringstream ss;
        if (hours > 0) {
            ss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            ss << minutes << "m " << seconds << "s";
        } else {
            ss << seconds << "s";
        }
        return ss.str();
    }
};

/**
 * @brief 总体统计信息
 */
struct OverallStats {
    std::chrono::milliseconds totalDuration;              // 总使用时长
    int totalSessionCount;                                 // 总会话次数
    std::chrono::system_clock::time_point firstUsage;     // 首次使用时间
    std::chrono::system_clock::time_point lastUsage;      // 最后使用时间
    int activeDays;                                        // 活跃天数
    
    OverallStats() : totalDuration(0), totalSessionCount(0), activeDays(0) {}
    
    // 获取格式化的总时长字符串
    std::string getFormattedTotalDuration() const {
        auto totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(totalDuration).count();
        auto hours = totalSeconds / 3600;
        auto minutes = (totalSeconds % 3600) / 60;
        auto seconds = totalSeconds % 60;
        
        std::stringstream ss;
        if (hours > 0) {
            ss << hours << "h " << minutes << "m " << seconds << "s";
        } else if (minutes > 0) {
            ss << minutes << "m " << seconds << "s";
        } else {
            ss << seconds << "s";
        }
        return ss.str();
    }
};

// ============================================================================
// 结构体定义
// ============================================================================

/**
 * @brief ASR 配置结构体
 * 
 * 注意：此配置结构体只包含业务逻辑相关的配置
 * 火山引擎ASR API相关的配置完全由AsrClient管理
 */
struct AsrConfig {
    // ============================================================================
    // 连接配置
    // ============================================================================
    std::string url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
    int handshakeTimeout = 30;  // 握手超时时间（秒）
    int connectionTimeout = 30; // 连接超时时间（秒）
    
    // ============================================================================
    // 认证配置
    // ============================================================================
    std::string appId;
    std::string accessToken;
    std::string secretKey;
    bool isValid = false;  // 配置是否有效
    std::string configSource;  // 配置来源："environment_variables", "trial_mode", "user_config"
    
    // ============================================================================
    // 客户端类型
    // ============================================================================
    ClientType clientType = ClientType::IXWEBSOCKET;
    
    // ============================================================================
    // 调试配置
    // ============================================================================
    
    // 日志级别 - 直接使用调试配置中的枚举
    AsrLogLevel logLevel = static_cast<AsrLogLevel>(ASR_LOG_LEVEL);
    
    // 调试开关 - 直接使用调试配置中的宏
    bool enableBusinessLog = ASR_ENABLE_BUSINESS_LOG;
    bool enableFlowLog = ASR_ENABLE_FLOW_LOG;
    bool enableDataLog = ASR_ENABLE_DATA_LOG;
    bool enableProtocolLog = ASR_ENABLE_PROTOCOL_LOG;
    bool enableAudioLog = ASR_ENABLE_AUDIO_LOG;
    
    // ============================================================================
    // 计时器配置
    // ============================================================================
    bool enableUsageTracking = true;                       // 是否启用使用统计
    std::string statsDataDir = "";                         // 统计数据存储目录
};

/**
 * @brief ASR 结果结构体
 */
struct AsrResult {
    std::string text;           // 识别的文本
    std::string logId;          // 日志 ID
    double confidence;          // 置信度
    std::map<std::string, std::string> metadata; // 元数据
};

/**
 * @brief 音频文件信息结构体
 */
struct AudioFileInfo {
    std::string format;      // 音频格式 (wav, pcm等)
    int sampleRate;          // 采样率
    int bitsPerSample;       // 位深度
    int channels;            // 声道数
    std::string codec;       // 编解码器
    size_t dataOffset;       // 音频数据偏移
    size_t dataSize;         // 音频数据大小
    double duration;         // 音频时长
    bool isValid;            // 是否有效
    
    AudioFileInfo() : sampleRate(0), bitsPerSample(0), channels(0), 
                     dataOffset(0), dataSize(0), duration(0.0), isValid(false) {}
};

// ============================================================================
// ASR 管理模块主类
// ============================================================================

/**
 * @brief ASR 管理模块主类
 * 
 * 提供统一的 ASR 接口，管理多种 ASR 客户端实现
 * 支持配置管理、连接控制、音频识别等功能
 * 新增：连接时间统计和持久化功能
 * 新增：单例模式支持，确保全局只有一个实例
 */
class AsrManager : public AsrCallback {
public:
    // ============================================================================
    // 单例模式
    // ============================================================================
    static AsrManager& instance();
    static void destroyInstance();
    
    // ============================================================================
    // 构造和析构
    // ============================================================================
    explicit AsrManager();
    ~AsrManager();

    // ============================================================================
    // 配置管理
    // ============================================================================
    void setConfig(const AsrConfig& config);
    AsrConfig getConfig() const;
    void setCallback(AsrCallback* callback);

    // ============================================================================
    // 状态查询
    // ============================================================================
    std::string getDetailedStatus() const;
    std::string getAudioStats() const;
    bool isConnected() const;
    AsrStatus getStatus() const;

    // ============================================================================
    // 连接控制
    // ============================================================================
    bool connect();
    void disconnect();

    // ============================================================================
    // 音频识别
    // ============================================================================
    bool sendAudio(const std::vector<uint8_t>& audioData, bool isLast = false);
    bool recognizeAudioFile(const std::string& filePath, bool waitForFinal = true, int timeoutMs = 30000);
    bool startRecognition();
    void stopRecognition();
    
    // ============================================================================
    // 音频文件处理
    // ============================================================================
    AudioFileInfo parseAudioFile(const std::string& filePath);

    // ============================================================================
    // 结果获取
    // ============================================================================
    AsrResult getLatestResult() const;
    std::vector<AsrResult> getAllResults() const;
    AsrResult getFinalResult() const;
    std::string getLogId() const;
    std::map<std::string, std::string> getResponseHeaders() const;
    
    // ============================================================================
    // 音频识别方法（异步）
    // ============================================================================
    void recognizeAudioFileAsync(const std::string& filePath);

    // ============================================================================
    // 计时器统计功能 (新增)
    // ============================================================================
    
    /**
     * @brief 获取今日使用统计
     * @return 今日统计信息
     */
    DailyUsageStats getTodayStats() const;
    
    /**
     * @brief 获取指定日期使用统计
     * @param date 日期字符串 (YYYY-MM-DD)
     * @return 指定日期统计信息
     */
    DailyUsageStats getDateStats(const std::string& date) const;
    
    /**
     * @brief 获取总体统计信息
     * @return 总体统计信息
     */
    OverallStats getOverallStats() const;
    
    /**
     * @brief 获取最近N天的统计信息
     * @param days 天数
     * @return 最近N天的统计信息列表
     */
    std::vector<DailyUsageStats> getRecentStats(int days) const;
    
    /**
     * @brief 获取所有历史统计信息
     * @return 所有历史统计信息
     */
    std::map<std::string, DailyUsageStats> getAllStats() const;
    
    /**
     * @brief 获取当前会话统计信息
     * @return 当前会话信息
     */
    ConnectionSession getCurrentSession() const;
    
    /**
     * @brief 保存统计数据到文件
     * @return 是否保存成功
     */
    bool saveStats();
    
    /**
     * @brief 从文件加载统计数据
     * @return 是否加载成功
     */
    bool loadStats();
    
    /**
     * @brief 清除所有统计数据
     */
    void clearStats();
    
    /**
     * @brief 导出统计数据为CSV格式
     * @param filePath 导出文件路径
     * @return 是否导出成功
     */
    bool exportToCsv(const std::string& filePath) const;
    
    /**
     * @brief 获取统计信息摘要字符串
     * @return 格式化的统计信息摘要
     */
    std::string getStatsSummary() const;
    
    // ============================================================================
    // 静态方法
    // ============================================================================
    static bool loadConfigFromEnv(AsrConfig& config);
    static std::string getStatusName(AsrStatus status);
    
    // 计时器相关静态方法
    static std::string getCurrentDate();
    static std::string generateSessionId();
    static std::string formatTimePoint(const std::chrono::system_clock::time_point& timePoint);

    // ============================================================================
    // AsrCallback 接口实现
    // ============================================================================
    void onOpen(AsrClient* client) override;
    void onMessage(AsrClient* client, const std::string& message) override;
    void onError(AsrClient* client, const std::string& error) override;
    void onClose(AsrClient* client) override;

    // 新增：测试ASR连接
    bool testConnection(const std::string& appId, const std::string& accessToken, const std::string& secretKey);

    // 新增：ASR线程状态判断和启动/关闭接口
    bool isAsrThreadRunning() const;
    void startAsrThread();
    void stopAsrThread();

private:
    // ============================================================================
    // 私有方法
    // ============================================================================
    std::unique_ptr<AsrClient> createClient(ClientType type);
    bool initializeClient();
    void updateStatus(AsrStatus status);
    AudioFileInfo parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header);
    AudioFileInfo parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header);
    void recognition_thread_func(const std::string& filePath);
    
    // 计时器相关私有方法
    void startSessionTimer();
    void endSessionTimer(bool isCompleted = true);
    void updateOverallStats();
    std::string getStatsFilePath() const;
    std::string getBackupFilePath() const;
    bool createDataDirectory() const;
    void logStats(const std::string& message) const;

    // ============================================================================
    // 私有成员变量
    // ============================================================================
    AsrConfig m_config;
    AsrStatus m_status;
    std::unique_ptr<AsrClient> m_client;
    AsrCallback* m_callback = nullptr;
    std::vector<AsrResult> m_results;
    AsrResult m_latestResult;
    std::vector<std::vector<uint8_t>> m_audioPackets;
    size_t m_audioSendIndex = 0;
    std::chrono::high_resolution_clock::time_point m_lastPacketTime;
    std::thread m_workerThread;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_stopFlag{false};

    // ============================================================================
    // 实时流相关成员变量 (新增) - 暂时注释掉，避免未使用警告
    // ============================================================================
    // bool isRealtimeStreaming_ = false;
    // bool isRealtimePaused_ = false;
    // std::vector<int16_t> realtimeAudioBuffer_;
    // size_t accumulatedFrames_ = 0;
    // size_t targetPacketFrames_ = 0;  // 100ms对应的帧数
    // mutable std::mutex realtimeMutex_;
    
    // ============================================================================
    // 计时器相关成员变量 (新增)
    // ============================================================================
    std::map<std::string, DailyUsageStats> m_dailyStats_;     // 每日统计信息
    ConnectionSession m_currentSession_;                       // 当前会话
    OverallStats m_overallStats_;                             // 总体统计信息
    std::atomic<int> m_sessionCounter_{0};                    // 会话计数器
    mutable std::mutex m_statsMutex_;                         // 统计信息互斥锁
    
    // 文件路径
    std::string m_statsFilePath_;                             // 统计文件路径
    std::string m_backupFilePath_;                            // 备份文件路径
};

} // namespace Asr 