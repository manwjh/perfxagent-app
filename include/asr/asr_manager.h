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
// 版本: 1.0.1
// 日期: 2024
// 参考: 火山引擎 ASR WebSocket 协议文档
//

#ifndef ASR_MANAGER_H
#define ASR_MANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace Asr {

// 前向声明
class AsrClient;
class AsrCallback;

/**
 * @brief ASR 客户端类型枚举
 */
enum class ClientType {
    IXWEBSOCKET = 1,    // 基于 IXWebSocket 的实现
    QT = 2,             // 基于 Qt 的实现
    WEBSOCKETPP = 3     // 基于 WebSocketpp 的实现
};

/**
 * @brief ASR 配置结构体
 */
struct AsrConfig {
    // 连接配置
    std::string url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
    int handshakeTimeout = 30;  // 握手超时时间（秒）
    int connectionTimeout = 30; // 连接超时时间（秒）
    
    // 认证配置
    std::string appId;
    std::string accessToken;
    std::string secretKey;
    bool isValid = false;  // 配置是否有效
    
    // 音频配置
    std::string format = "wav";
    int sampleRate = 16000;
    int bits = 16;
    int channels = 1;
    std::string codec = "raw";
    
    // 请求配置
    std::string uid = "test";
    std::string language = "zh-CN";
    std::string resultType = "full";
    bool streaming = true;
    int segDuration = 100;
    
    // 客户端类型
    ClientType clientType = ClientType::IXWEBSOCKET;
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
 * @brief ASR 管理模块主类
 * 
 * 提供统一的 ASR 接口，管理多种 ASR 客户端实现
 */
class AsrManager {
public:
    /**
     * @brief 构造函数
     */
    explicit AsrManager();
    
    /**
     * @brief 析构函数
     */
    ~AsrManager();
    
    // ============================================================================
    // 配置方法
    // ============================================================================
    
    /**
     * @brief 设置配置
     * @param config ASR 配置
     */
    void setConfig(const AsrConfig& config);
    
    /**
     * @brief 获取配置
     * @return ASR 配置
     */
    AsrConfig getConfig() const;
    
    /**
     * @brief 设置客户端类型
     * @param type 客户端类型
     */
    void setClientType(ClientType type);
    
    /**
     * @brief 设置回调接口
     * @param callback 回调接口指针
     */
    void setCallback(AsrCallback* callback);
    
    // ============================================================================
    // 连接控制方法
    // ============================================================================
    
    /**
     * @brief 连接到 ASR 服务器
     * @return 是否成功启动连接
     */
    bool connect();
    
    /**
     * @brief 断开连接
     */
    void disconnect();
    
    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    bool isConnected() const;
    
    /**
     * @brief 获取当前状态
     * @return ASR 状态
     */
    AsrStatus getStatus() const;
    
    // ============================================================================
    // 音频识别方法
    // ============================================================================
    
    /**
     * @brief 发送音频数据
     * @param audioData 音频数据
     * @param isLast 是否为最后一段
     * @return 是否发送成功
     */
    bool sendAudio(const std::vector<uint8_t>& audioData, bool isLast = false);
    
    /**
     * @brief 发送音频文件
     * @param filePath 音频文件路径
     * @return 是否发送成功
     */
    bool sendAudioFile(const std::string& filePath);
    
    /**
     * @brief 识别音频文件（完整流程）
     * @param filePath 音频文件路径
     * @param waitForFinal 是否等待最终结果
     * @param timeoutMs 超时时间（毫秒）
     * @return 是否识别成功
     */
    bool recognizeAudioFile(const std::string& filePath, bool waitForFinal = true, int timeoutMs = 30000);
    
    /**
     * @brief 获取音频文件信息
     * @param filePath 音频文件路径
     * @return 音频文件信息结构体
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
    
    AudioFileInfo parseAudioFile(const std::string& filePath);
    
    /**
     * @brief 开始识别
     * @return 是否成功开始识别
     */
    bool startRecognition();
    
    /**
     * @brief 停止识别
     */
    void stopRecognition();
    
    // ============================================================================
    // 结果获取方法
    // ============================================================================
    
    /**
     * @brief 获取最新的识别结果
     * @return 识别结果
     */
    AsrResult getLatestResult() const;
    
    /**
     * @brief 获取所有识别结果
     * @return 识别结果列表
     */
    std::vector<AsrResult> getAllResults() const;
    
    /**
     * @brief 获取日志 ID
     * @return 日志 ID
     */
    std::string getLogId() const;
    
    /**
     * @brief 获取响应头
     * @return 响应头映射
     */
    std::map<std::string, std::string> getResponseHeaders() const;
    
    // ============================================================================
    // 静态方法
    // ============================================================================
    
    /**
     * @brief 从环境变量加载配置
     * @return 是否成功加载
     */
    static bool loadConfigFromEnv(AsrConfig& config);
    
    /**
     * @brief 获取客户端类型名称
     * @param type 客户端类型
     * @return 客户端类型名称
     */
    static std::string getClientTypeName(ClientType type);
    
    /**
     * @brief 获取状态名称
     * @param status ASR 状态
     * @return 状态名称
     */
    static std::string getStatusName(AsrStatus status);

    bool sendNextAudioPacket(); // 发送下一个音频包

    // 音频包ACK回调
    void onAudioAck();

    // 检查是否还有更多音频包
    bool hasMoreAudioPackets() const;

private:
    // ============================================================================
    // 私有方法
    // ============================================================================
    
    /**
     * @brief 创建客户端实例
     * @param type 客户端类型
     * @return 客户端实例指针
     */
    std::unique_ptr<AsrClient> createClient(ClientType type);
    
    /**
     * @brief 初始化客户端
     * @return 是否成功初始化
     */
    bool initializeClient();
    
    /**
     * @brief 更新状态
     * @param status 新状态
     */
    void updateStatus(AsrStatus status);
    
    /**
     * @brief 解析WAV文件
     * @param filePath 文件路径
     * @param header 文件头数据
     * @return 音频文件信息
     */
    AudioFileInfo parseWavFile(const std::string& filePath, const std::vector<uint8_t>& header);
    
    /**
     * @brief 解析MP3文件
     * @param filePath 文件路径
     * @param header 文件头数据
     * @return 音频文件信息
     */
    AudioFileInfo parseMp3File(const std::string& filePath, const std::vector<uint8_t>& header);
    
    /**
     * @brief 解析PCM文件
     * @param filePath 文件路径
     * @param header 文件头数据
     * @return 音频文件信息
     */
    AudioFileInfo parsePcmFile(const std::string& filePath, const std::vector<uint8_t>& header);

private:
    // ============================================================================
    // 私有成员变量
    // ============================================================================
    
    // 配置
    AsrConfig m_config;
    
    // 状态
    AsrStatus m_status;
    
    // 客户端实例
    std::unique_ptr<AsrClient> m_client;
    
    // 回调接口
    AsrCallback* m_callback;
    
    // 识别结果
    std::vector<AsrResult> m_results;
    AsrResult m_latestResult;
    
    std::vector<std::vector<uint8_t>> m_audioPackets; // 音频分包缓存
    size_t m_audioSendIndex = 0; // 当前发送到第几个包
};

} // namespace Asr

#endif // ASR_MANAGER_H 