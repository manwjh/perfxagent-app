//
// 基于 IXWebSocket 的 ASR 客户端头文件
// 
// 本文件定义了基于 IXWebSocket 的 ASR（自动语音识别）WebSocket 客户端接口
// 完全兼容火山引擎 ASR WebSocket 协议，支持流式音频识别
// 
// 主要功能：
// - WebSocket 连接管理
// - 音频数据流式传输
// - 协议消息封装和解析
// - 异步回调处理
// - 环境变量凭据管理
// 
// 作者: PerfXAgent Team
// 版本: 1.0.1
// 日期: 2024
// 参考: 火山引擎 ASR WebSocket 协议文档
//

#ifndef ASR_CLIENT_H
#define ASR_CLIENT_H

#include <ixwebsocket/IXWebSocket.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <zlib.h>

using json = nlohmann::json;

namespace Asr {

// ============================================================================
// 协议常量定义 - 火山引擎 ASR WebSocket 协议
// ============================================================================

// 协议版本
const uint8_t PROTOCOL_VERSION = 0x01;

// 消息类型 (4 bits)
const uint8_t FULL_CLIENT_REQUEST = 0x01;      // 完整客户端请求
const uint8_t AUDIO_ONLY_REQUEST = 0x02;       // 音频数据请求
const uint8_t FULL_SERVER_RESPONSE = 0x09;     // 完整服务器响应
const uint8_t ERROR_RESPONSE = 0x0F;           // 错误响应

// 消息类型特定标志 (4 bits)
const uint8_t POS_SEQUENCE = 0x00;             // 正数序列号
const uint8_t NEG_WITH_SEQUENCE = 0x02;        // 负数序列号（最后一包）
const uint8_t NONE_FLAG = 0x01;                // 无特殊标志

// 序列化方法 (4 bits)
const uint8_t JSON_SERIALIZATION = 0x01;       // JSON 序列化
const uint8_t RAW_BYTES = 0x00;                // 原始字节

// 压缩类型 (4 bits)
const uint8_t GZIP_COMPRESSION = 0x01;         // GZIP 压缩
const uint8_t NO_COMPRESSION = 0x00;           // 无压缩

// ============================================================================
// 错误码定义 - 火山引擎 ASR 官方错误码
// ============================================================================

// 成功
const uint32_t ERROR_SUCCESS = 20000000;

// 客户端错误 (45xxxxxx)
const uint32_t ERROR_INVALID_PARAMS = 45000001;    // 请求参数无效，请求参数缺失必需字段 / 字段值无效 / 重复请求
const uint32_t ERROR_EMPTY_AUDIO = 45000002;       // 空音频
const uint32_t ERROR_PACKET_TIMEOUT = 45000081;    // 等包超时
const uint32_t ERROR_INVALID_AUDIO_FORMAT = 45000151; // 音频格式不正确
const uint32_t ERROR_UNAUTHORIZED = 45000003;      // 未授权，认证失败
const uint32_t ERROR_FORBIDDEN = 45000004;         // 禁止访问，权限不足
const uint32_t ERROR_NOT_FOUND = 45000005;         // 资源不存在
const uint32_t ERROR_TIMEOUT = 45000006;           // 超时
const uint32_t ERROR_UNKNOWN = 45000007;           // 未知错误

// 服务器错误 (55xxxxxx)
const uint32_t ERROR_SERVER_BUSY = 55000031;       // 服务器繁忙，服务过载，无法处理当前请求
const uint32_t ERROR_SERVER_ERROR = 55000001;      // 服务器内部错误
const uint32_t ERROR_SERVICE_UNAVAILABLE = 55000002; // 服务不可用
const uint32_t ERROR_QUOTA_EXCEEDED = 55000003;    // 配额超限
const uint32_t ERROR_RATE_LIMITED = 55000004;      // 请求频率超限

// 错误码范围
const uint32_t ERROR_CLIENT_RANGE_START = 45000000;  // 客户端错误起始
const uint32_t ERROR_CLIENT_RANGE_END = 45999999;    // 客户端错误结束
const uint32_t ERROR_SERVER_RANGE_START = 55000000;  // 服务器错误起始
const uint32_t ERROR_SERVER_RANGE_END = 55999999;    // 服务器错误结束

/**
 * @brief ASR 错误信息结构体
 */
struct AsrError {
    uint32_t code;           // 错误码
    std::string message;     // 错误消息
    std::string details;     // 详细错误信息
    
    AsrError() : code(0) {}
    AsrError(uint32_t c, const std::string& msg, const std::string& det = "") 
        : code(c), message(msg), details(det) {}
    
    bool isSuccess() const { return code == ERROR_SUCCESS; }
    bool isClientError() const { return code >= ERROR_CLIENT_RANGE_START && code <= ERROR_CLIENT_RANGE_END; }
    bool isServerError() const { return code >= ERROR_SERVER_RANGE_START && code <= ERROR_SERVER_RANGE_END; }
    
    std::string getErrorDescription() const {
        switch (code) {
            case ERROR_SUCCESS:
                return "成功";
            case ERROR_INVALID_PARAMS:
                return "请求参数无效，请求参数缺失必需字段 / 字段值无效 / 重复请求";
            case ERROR_EMPTY_AUDIO:
                return "空音频";
            case ERROR_PACKET_TIMEOUT:
                return "等包超时";
            case ERROR_INVALID_AUDIO_FORMAT:
                return "音频格式不正确";
            case ERROR_SERVER_BUSY:
                return "服务器繁忙，服务过载，无法处理当前请求";
            default:
                if (isClientError()) {
                    return "客户端错误";
                } else if (isServerError()) {
                    return "服务内部处理错误";
                } else {
                    return "未知错误";
                }
        }
    }
};

// 前向声明
class AsrClient;

/**
 * @brief ASR 客户端回调接口
 * 
 * 定义 ASR 客户端的事件回调接口，用于处理连接状态变化和消息接收
 */
class AsrCallback {
public:
    virtual ~AsrCallback() = default;
    
    /**
     * @brief WebSocket 连接建立回调
     * @param client ASR 客户端指针
     */
    virtual void onOpen(AsrClient* client) = 0;
    
    /**
     * @brief 收到服务器消息回调
     * @param client ASR 客户端指针
     * @param message 接收到的消息内容
     */
    virtual void onMessage(AsrClient* client, const std::string& message) = 0;
    
    /**
     * @brief 发生错误回调
     * @param client ASR 客户端指针
     * @param error 错误信息
     */
    virtual void onError(AsrClient* client, const std::string& error) = 0;
    
    /**
     * @brief 连接关闭回调
     * @param client ASR 客户端指针
     */
    virtual void onClose(AsrClient* client) = 0;
};

/**
 * @brief ASR 客户端主类
 * 
 * 实现基于 IXWebSocket 的 ASR WebSocket 客户端，支持流式音频识别
 * 完全兼容火山引擎 ASR 协议，提供完整的音频识别功能
 */
class AsrClient {
public:
    /**
     * @brief 认证类型枚举
     */
    enum class AuthType {
        TOKEN = 1,      // 令牌认证
        SIGNATURE = 2   // 签名认证
    };

    /**
     * @brief 音频类型枚举
     */
    enum class AudioType {
        LOCAL = 1,  // 本地音频文件
        URL = 2     // 远程音频 URL
    };

    /**
     * @brief 凭据结构体
     * 
     * 存储 ASR 服务的认证信息
     */
    struct Credentials {
        std::string appId;        // 应用 ID
        std::string accessToken;  // 访问令牌
        std::string secretKey;    // 密钥
        bool isValid = false;     // 是否有效
    };

    /**
     * @brief 构造函数
     */
    explicit AsrClient();
    
    /**
     * @brief 析构函数
     */
    ~AsrClient();

    // ============================================================================
    // 配置方法 - 设置客户端参数
    // ============================================================================

    /**
     * @brief 设置应用 ID
     * @param appId 应用 ID
     */
    void setAppId(const std::string& appId);
    
    /**
     * @brief 设置访问令牌
     * @param token 访问令牌
     */
    void setToken(const std::string& token);
    
    /**
     * @brief 设置密钥
     * @param secretKey 密钥
     */
    void setSecretKey(const std::string& secretKey);
    
    /**
     * @brief 设置认证类型
     * @param authType 认证类型
     */
    void setAuthType(AuthType authType);
    
    /**
     * @brief 设置音频格式参数
     * @param format 音频格式 (wav, mp3, pcm 等)
     * @param channels 声道数
     * @param sampleRate 采样率
     * @param bits 位深度
     */
    void setAudioFormat(const std::string& format, int channels, int sampleRate, int bits);
    
    /**
     * @brief 设置集群名称
     * @param cluster 集群名称
     */
    void setCluster(const std::string& cluster);
    
    /**
     * @brief 设置回调接口
     * @param callback 回调接口指针
     */
    void setCallback(AsrCallback* callback);
    
    /**
     * @brief 设置用户 ID
     * @param uid 用户 ID
     */
    void setUid(const std::string& uid);
    
    /**
     * @brief 设置语言
     * @param language 语言代码
     */
    void setLanguage(const std::string& language);
    
    /**
     * @brief 设置结果类型
     * @param resultType 结果类型
     */
    void setResultType(const std::string& resultType);
    
    /**
     * @brief 设置是否启用流式传输
     * @param streaming 是否启用流式传输
     */
    void setStreaming(bool streaming);
    
    /**
     * @brief 设置分段时长
     * @param duration 分段时长（毫秒）
     */
    void setSegDuration(int duration);

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

    // ============================================================================
    // 音频发送方法
    // ============================================================================

    /**
     * @brief 发送音频数据
     * @param audioData 音频数据
     * @param sequence 序列号（负数表示最后一个包）
     * @return 是否发送成功
     */
    bool sendAudio(const std::vector<uint8_t>& audioData, int32_t sequence);
    
    /**
     * @brief 发送音频文件
     * @param filePath 音频文件路径
     * @return 是否发送成功
     */
    bool sendAudioFile(const std::string& filePath);

    // ============================================================================
    // 请求发送方法
    // ============================================================================

    /**
     * @brief 发送完整客户端请求并等待响应
     * @param timeoutMs 超时时间（毫秒）
     * @param response 响应字符串指针，可为 nullptr
     * @return 是否成功接收到响应
     */
    bool sendFullClientRequestAndWaitResponse(int timeoutMs = 5000, std::string* response = nullptr);
    
    /**
     * @brief 发送完整客户端请求并获取响应
     * @param timeoutMs 超时时间（毫秒）
     * @return 响应字符串
     */
    std::string sendFullClientRequestAndGetResponse(int timeoutMs = 5000);

    // ============================================================================
    // 获取状态信息
    // ============================================================================

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
    
    /**
     * @brief 获取完整客户端请求的 JSON 字符串
     * @return JSON 字符串
     */
    std::string getFullClientRequestJson() const;

    // ============================================================================
    // 错误处理方法
    // ============================================================================

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息结构体
     */
    AsrError getLastError() const;
    
    /**
     * @brief 解析服务器错误响应
     * @param response 服务器响应字符串
     * @return 解析后的错误信息
     */
    static AsrError parseErrorResponse(const std::string& response);
    
    /**
     * @brief 检查响应是否包含错误
     * @param response 响应字符串
     * @return 是否包含错误
     */
    static bool hasError(const std::string& response);
    
    /**
     * @brief 获取错误码描述
     * @param errorCode 错误码
     * @return 错误描述
     */
    static std::string getErrorDescription(uint32_t errorCode);

    // ============================================================================
    // 静态方法
    // ============================================================================

    /**
     * @brief 从环境变量获取凭据
     * @return 凭据结构体
     */
    static Credentials getCredentialsFromEnv();

    bool isReadyForAudio() const { return m_readyForAudio; }

private:
    // ============================================================================
    // 私有方法
    // ============================================================================

    /**
     * @brief 更新 HTTP 头部
     */
    void updateHeaders();
    
    /**
     * @brief 处理 WebSocket 消息
     * @param msg WebSocket 消息指针
     */
    void handleMessage(const ix::WebSocketMessagePtr& msg);
    
    /**
     * @brief 构造请求 JSON
     * @return 请求 JSON 对象
     */
    json constructRequest() const;
    
    /**
     * @brief 生成协议头部
     * @param messageType 消息类型
     * @param messageTypeSpecificFlags 消息类型特定标志
     * @param serialMethod 序列化方法
     * @param compressionType 压缩类型
     * @param reservedData 保留数据
     * @return 4 字节的协议头部
     */
    std::vector<uint8_t> generateHeader(uint8_t messageType, uint8_t messageTypeSpecificFlags, 
                                       uint8_t serialMethod, uint8_t compressionType, uint8_t reservedData);
    
    /**
     * @brief 生成序列号前缀
     * @param sequence 序列号
     * @return 序列号前缀字节数组
     */
    std::vector<uint8_t> generateBeforePayload(int32_t sequence);
    
    /**
     * @brief 解析二进制响应协议
     * @param binaryData 二进制数据
     * @return 解析后的 JSON 字符串
     */
    std::string parseBinaryResponse(const std::string& binaryData);
    
    /**
     * @brief GZIP 压缩字符串
     * @param data 原始字符串
     * @return 压缩后的数据
     */
    std::vector<uint8_t> gzipCompress(const std::string& data);
    
    /**
     * @brief GZIP 压缩字节数组
     * @param data 原始字节数组
     * @return 压缩后的数据
     */
    std::vector<uint8_t> gzipCompress(const std::vector<uint8_t>& data);
    
    /**
     * @brief Gzip 解压缩
     * @param data 要解压缩的数据
     * @return 解压缩后的字符串
     */
    std::string gzipDecompress(const std::string& data);
    
    /**
     * @brief 等待响应
     * @param timeoutMs 超时时间（毫秒）
     * @param response 响应字符串指针，可为 nullptr
     * @return 是否成功接收到响应
     */
    bool waitForResponse(int timeoutMs, std::string* response = nullptr);
    
    /**
     * @brief 生成 UUID
     * @return UUID 字符串
     */
    std::string generateUuid();

private:
    // ============================================================================
    // 私有成员变量
    // ============================================================================

    // WebSocket 相关
    ix::WebSocket m_webSocket;
    bool m_connected;
    
    // 配置参数
    std::string m_appId;
    std::string m_accessToken;
    std::string m_secretKey;
    AuthType m_authType;
    std::string m_cluster;
    
    // 音频参数
    std::string m_format;
    int m_sampleRate;
    int m_bits;
    int m_channels;
    std::string m_codec;
    
    // 请求参数
    std::string m_reqId;
    int32_t m_seq;
    std::string m_uid;
    std::string m_language;
    std::string m_resultType;
    bool m_streaming;
    int m_segDuration;
    
    // 状态信息
    std::string m_logId;
    std::map<std::string, std::string> m_responseHeaders;
    
    // 回调接口
    AsrCallback* m_callback;

    bool m_readyForAudio = false;
    
    // 错误处理
    AsrError m_lastError;

    bool m_firstAudioSent = false;
};

} // namespace Asr

#endif // ASR_CLIENT_H 