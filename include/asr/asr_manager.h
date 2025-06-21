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
// 
// 作者: PerfXAgent Team
// 版本: 1.2.0
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
#include "asr/asr_client.h"
#include "asr/asr_debug_config.h"

namespace Asr {

// ============================================================================
// 枚举定义
// ============================================================================

/**
 * @brief ASR 客户端类型枚举
 */
enum class ClientType {
    IXWEBSOCKET = 1,    // 基于 IXWebSocket 的实现
    QT = 2,             // 基于 Qt 的实现
    WEBSOCKETPP = 3     // 基于 WebSocketpp 的实现
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
// 结构体定义
// ============================================================================

/**
 * @brief ASR 配置结构体
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
    
    // ============================================================================
    // 音频配置
    // ============================================================================
    std::string format = "wav";
    int sampleRate = 16000;
    int bits = 16;
    int channels = 1;
    std::string codec = "raw";
    
    // ============================================================================
    // 请求配置
    // ============================================================================
    std::string uid = "test";
    std::string language = "zh-CN";
    std::string resultType = "full";
    bool streaming = true;
    int segDuration = 100;
    
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
};

/**
 * @brief ASR 结果结构体
 */
struct AsrResult {
    std::string text;           // 识别的文本
    std::string logId;          // 日志 ID
    bool isFinal;               // 是否为最终结果
    double confidence;          // 置信度
    std::map<std::string, std::string> metadata; // 元数据
};

/**
 * @brief 音频文件信息结构体
 */
struct AudioFileInfo {
    std::string format;      // 音频格式 (wav, mp3, pcm等)
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
 */
class AsrManager : public AsrCallback {
public:
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
    void setClientType(ClientType type);
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
    bool sendAudioFile(const std::string& filePath);
    bool recognizeAudioFile(const std::string& filePath, bool waitForFinal = true, int timeoutMs = 30000);
    bool startRecognition();

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
    // 音频包管理
    // ============================================================================
    bool sendNextAudioPacket();
    void onAudioAck();
    bool hasMoreAudioPackets() const;
    bool isAudioPacketSendingComplete() const;
    std::string getAudioPacketStatus() const;

    // ============================================================================
    // 音频识别方法（异步）
    // ============================================================================
    void recognizeAudioFileAsync(const std::string& filePath);
    void stopRecognition();
    
    // ============================================================================
    // 静态方法
    // ============================================================================
    static bool loadConfigFromEnv(AsrConfig& config);
    static std::string getClientTypeName(ClientType type);
    static std::string getStatusName(AsrStatus status);

    // ============================================================================
    // AsrCallback 接口实现
    // ============================================================================
    void onOpen(AsrClient* client) override;
    void onMessage(AsrClient* client, const std::string& message) override;
    void onError(AsrClient* client, const std::string& error) override;
    void onClose(AsrClient* client) override;

private:
    // ============================================================================
    // 私有方法
    // ============================================================================
    std::unique_ptr<AsrClient> createClient(ClientType type);
    bool initializeClient();
    void updateStatus(AsrStatus status);
    AudioFileInfo parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header);
    AudioFileInfo parseMp3File(const std::string& filePath, const std::vector<uint8_t>& header);
    AudioFileInfo parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header);
    void recognition_thread_func(const std::string& filePath);

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
};

} // namespace Asr 