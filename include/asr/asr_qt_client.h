//
// ASR Qt客户端头文件
// 
// 本文件定义了基于Qt的ASR（自动语音识别）WebSocket客户端接口
// 完全兼容火山引擎ASR WebSocket协议，支持流式音频识别
// 
// 主要功能：
// - WebSocket连接管理
// - 音频数据流式传输
// - 协议消息封装和解析
// - 异步回调处理
// - 环境变量凭据管理
// 
// 作者: PerfXAgent Team
// 版本: 1.2.0
// 日期: 2024
// 参考: 火山引擎ASR WebSocket协议文档
//

#ifndef ASR_QT_CLIENT_H
#define ASR_QT_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QWebSocket>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QMutex>
#include <QTimer>
#include <QVariantMap>
#include <memory>

namespace Asr {

// 前向声明
class AsrQtClient;

/**
 * @brief ASR客户端回调接口
 * 
 * 定义ASR客户端的事件回调接口，用于处理连接状态变化和消息接收
 */
class AsrQtCallback {
public:
    virtual ~AsrQtCallback() = default;
    
    /**
     * @brief WebSocket连接建立回调
     * @param client ASR客户端指针
     */
    virtual void onOpen(AsrQtClient* client) = 0;
    
    /**
     * @brief 收到服务器消息回调
     * @param client ASR客户端指针
     * @param message 接收到的消息内容
     */
    virtual void onMessage(AsrQtClient* client, const QString& message) = 0;
    
    /**
     * @brief 发生错误回调
     * @param client ASR客户端指针
     * @param error 错误信息
     */
    virtual void onError(AsrQtClient* client, const QString& error) = 0;
    
    /**
     * @brief 连接关闭回调
     * @param client ASR客户端指针
     */
    virtual void onClose(AsrQtClient* client) = 0;
};

/**
 * @brief ASR Qt客户端主类
 * 
 * 实现基于Qt的ASR WebSocket客户端，支持流式音频识别
 * 完全兼容火山引擎ASR协议，提供完整的音频识别功能
 */
class AsrQtClient : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 认证类型枚举
     */
    enum AuthType {
        TOKEN = 1,      // 令牌认证
        SIGNATURE = 2   // 签名认证
    };

    /**
     * @brief 音频类型枚举
     */
    enum AudioType {
        LOCAL = 1,  // 本地音频文件
        URL = 2     // 远程音频URL
    };

    /**
     * @brief 消息类型枚举 (4 bits)
     * 
     * 定义ASR协议中的消息类型，与官方Python示例保持一致
     */
    enum MessageType : uint8_t {
        FULL_CLIENT_REQUEST = 0b0001,        // 完整客户端请求
        AUDIO_ONLY_CLIENT_REQUEST = 0b0010,  // 仅音频客户端请求
        FULL_SERVER_RESPONSE = 0b1001,       // 完整服务器响应
        SERVER_ACK = 0b1011,                 // 服务器确认
        ERROR_MESSAGE_FROM_SERVER = 0b1111   // 服务器错误消息
    };

    /**
     * @brief 消息类型标志枚举 (4 bits)
     * 
     * 定义序列号相关的标志位
     */
    enum MessageTypeFlag : uint8_t {
        NO_SEQUENCE_NUMBER = 0b0000,              // 无序列号
        POSITIVE_SEQUENCE_CLIENT_ASSIGN = 0b0001, // 正序列号（客户端分配）
        NEGATIVE_SEQUENCE_SERVER_ASSIGN = 0b0010, // 负序列号（服务器分配）
        NEGATIVE_SEQUENCE_CLIENT_ASSIGN = 0b0011  // 负序列号（客户端分配）
    };

    /**
     * @brief 消息序列化方法枚举 (4 bits)
     */
    enum MessageSerial : uint8_t {
        NO_SERIAL = 0b0000,      // 无序列化
        JSON = 0b0001,           // JSON序列化
        CUSTOM_SERIAL = 0b1111   // 自定义序列化
    };

    /**
     * @brief 消息压缩方法枚举 (4 bits)
     */
    enum MessageCompress : uint8_t {
        NO_COMPRESS = 0b0000,        // 无压缩
        GZIP = 0b0001,               // GZIP压缩
        CUSTOM_COMPRESS = 0b1111     // 自定义压缩
    };

    /**
     * @brief 凭据结构体
     * 
     * 存储ASR服务的认证信息
     */
    struct Credentials {
        QString appId;        // 应用ID
        QString accessToken;  // 访问令牌
        QString secretKey;    // 密钥
        bool isValid = false; // 是否有效
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit AsrQtClient(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     */
    ~AsrQtClient();

    // ============================================================================
    // 配置方法 - 设置客户端参数
    // ============================================================================

    /**
     * @brief 设置应用ID
     * @param appId 应用ID
     */
    void setAppId(const QString& appId);
    
    /**
     * @brief 设置访问令牌
     * @param token 访问令牌
     */
    void setToken(const QString& token);
    
    /**
     * @brief 设置密钥
     * @param secretKey 密钥
     */
    void setSecretKey(const QString& secretKey);
    
    /**
     * @brief 设置认证类型
     * @param authType 认证类型
     */
    void setAuthType(AuthType authType);
    
    /**
     * @brief 设置音频格式参数
     * @param format 音频格式 (wav, mp3, pcm等)
     * @param channels 声道数
     * @param sampleRate 采样率
     * @param bits 位深度
     */
    void setAudioFormat(const QString& format, int channels, int sampleRate, int bits);
    
    /**
     * @brief 设置集群信息
     * @param cluster 集群名称
     */
    void setCluster(const QString& cluster);
    
    /**
     * @brief 设置回调对象
     * @param callback 回调对象指针
     */
    void setCallback(AsrQtCallback* callback);
    
    /**
     * @brief 设置用户ID
     * @param uid 用户ID
     */
    void setUid(const QString& uid);
    
    /**
     * @brief 设置工作流
     * @param workflow 工作流字符串
     */
    void setWorkflow(const QString& workflow);
    
    /**
     * @brief 设置序列号
     * @param seq 序列号
     */
    void setSequence(int seq);
    
    /**
     * @brief 设置N-best结果数量
     * @param nbest N-best数量
     */
    void setNbest(int nbest);
    
    /**
     * @brief 设置是否显示话语信息
     * @param show 是否显示
     */
    void setShowUtterances(bool show);
    
    /**
     * @brief 设置是否显示语言信息
     * @param show 是否显示
     */
    void setShowLanguage(bool show);
    
    /**
     * @brief 设置结果类型
     * @param resultType 结果类型
     */
    void setResultType(const QString& resultType);
    
    /**
     * @brief 设置语言
     * @param language 语言代码
     */
    void setLanguage(const QString& language);
    
    /**
     * @brief 设置分段时长
     * @param duration 分段时长(毫秒)
     */
    void setSegDuration(int duration);
    
    /**
     * @brief 设置是否启用流式传输
     * @param streaming 是否启用流式传输
     */
    void setStreaming(bool streaming);

    // ============================================================================
    // 连接控制方法
    // ============================================================================

    /**
     * @brief 连接到ASR服务器
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
    // 音频数据发送方法
    // ============================================================================

    /**
     * @brief 发送音频数据
     * @param audioData 音频数据
     * @param isLast 是否为最后一个包
     * @return 是否发送成功
     */
    bool sendAudio(const QByteArray& audioData, bool isLast = false);
    
    /**
     * @brief 发送音频文件
     * @param filePath 音频文件路径
     * @return 是否发送成功
     */
    bool sendAudioFile(const QString& filePath);

    // ============================================================================
    // 完整客户端请求方法
    // ============================================================================

    /**
     * @brief 发送完整客户端请求并等待响应
     * @param timeoutMs 超时时间（毫秒），默认5000ms
     * @param response 响应字符串指针，可为nullptr
     * @return 是否成功
     */
    bool sendFullClientRequestAndWaitResponse(int timeoutMs = 5000, QString* response = nullptr);
    
    /**
     * @brief 发送完整客户端请求并获取响应
     * @param timeoutMs 超时时间（毫秒），默认5000ms
     * @return 响应字符串，失败时返回空字符串
     */
    QString sendFullClientRequestAndGetResponse(int timeoutMs = 5000);

    /**
     * @brief 获取完整客户端请求的JSON字符串
     * @return JSON格式的请求字符串
     * 
     * 构建符合火山引擎ASR协议的请求JSON
     */
    QString getFullClientRequestJson() const;

    /**
     * @brief 获取服务器返回的logid
     * @return logid字符串，如果未获取到则返回空字符串
     * 
     * 获取X-Tt-Logid，用于排错和问题定位
     */
    QString getLogid() const { return m_logid; }

    // ============================================================================
    // 静态工具方法
    // ============================================================================

    /**
     * @brief 从环境变量获取凭据
     * @return 凭据结构体
     * 
     * 从环境变量读取ASR认证信息：
     * - ASR_APP_ID: 应用ID
     * - ASR_ACCESS_TOKEN: 访问令牌
     * - ASR_SECRET_KEY: 密钥
     */
    static Credentials getCredentialsFromEnv();

signals:
    /**
     * @brief 连接建立信号
     */
    void connected();
    
    /**
     * @brief 连接断开信号
     */
    void disconnected();
    
    /**
     * @brief 消息接收信号
     * @param message 接收到的消息
     */
    void messageReceived(const QString& message);
    
    /**
     * @brief 错误发生信号
     * @param error 错误信息
     */
    void errorOccurred(const QString& error);

private slots:
    /**
     * @brief WebSocket连接建立槽函数
     */
    void onWebSocketConnected();
    
    /**
     * @brief WebSocket连接断开槽函数
     */
    void onWebSocketDisconnected();
    
    /**
     * @brief WebSocket错误槽函数
     * @param error 错误类型
     */
    void onWebSocketError(QAbstractSocket::SocketError error);
    
    /**
     * @brief WebSocket文本消息接收槽函数
     * @param message 接收到的文本消息
     */
    void onWebSocketTextMessageReceived(const QString& message);
    
    /**
     * @brief WebSocket二进制消息接收槽函数
     * @param message 接收到的二进制消息
     */
    void onWebSocketBinaryMessageReceived(const QByteArray& message);
    
    /**
     * @brief 连接超时槽函数
     */
    void onConnectionTimeout();

private:
    // ============================================================================
    // 内部协议处理方法
    // ============================================================================

    /**
     * @brief 发送初始参数
     * @return 是否发送成功
     */
    bool sendParams();
    
    /**
     * @brief 解析服务器响应
     * @param response 原始响应数据
     * @return 解析后的响应映射
     */
    QVariantMap parseResponse(const QByteArray& response);
    
    /**
     * @brief 生成协议头部
     * @param messageType 消息类型
     * @param messageTypeSpecificFlags 消息类型特定标志
     * @param serialMethod 序列化方法
     * @param compressionType 压缩类型
     * @param reservedData 保留数据
     * @return 4字节的协议头部
     */
    QByteArray generateHeader(uint8_t messageType, uint8_t messageTypeSpecificFlags, 
                             uint8_t serialMethod, uint8_t compressionType, uint8_t reservedData);
    
    /**
     * @brief 生成序列号前的内容
     * @param sequence 序列号
     * @return 4字节的序列号数据（大端序，有符号）
     */
    QByteArray generateBeforePayload(int32_t sequence);
    
    /**
     * @brief 等待响应
     * @param timeoutMs 超时时间（毫秒）
     * @param response 响应字符串指针，可为nullptr
     * @return 是否成功接收到响应
     */
    bool waitForResponse(int timeoutMs, QString* response = nullptr);
    
    /**
     * @brief GZIP压缩
     * @param data 原始数据
     * @return 压缩后的数据
     */
    QByteArray gzipCompress(const QByteArray& data);
    
    /**
     * @brief GZIP解压缩
     * @param data 压缩数据
     * @return 解压缩后的数据
     */
    QByteArray gzipDecompress(const QByteArray& data);

private:
    // ============================================================================
    // 私有成员变量
    // ============================================================================

    // 配置参数
    QString m_url{"wss://openspeech.bytedance.com/api/v3/sauc/bigmodel"};  // ASR服务器URL
    QString m_appId;        // 应用ID
    QString m_token;        // 访问令牌
    QString m_secretKey;    // 密钥
    AuthType m_authType{TOKEN};  // 认证类型
    QString m_cluster;      // 集群名称
    
    // 音频参数
    QString m_format{"wav"};    // 音频格式
    int m_sampleRate{16000};    // 采样率
    int m_bits{16};             // 位深度
    int m_channels{1};          // 声道数
    QString m_codec{"raw"};     // 编解码器
    
    // 请求参数
    QString m_reqId;        // 请求ID
    int32_t m_seq{1};       // 序列号
    QString m_uid{"test"};  // 用户ID
    QString m_workflow{"audio_in,resample,partition,vad,fe,decode"};  // 工作流
    int m_nbest{1};         // N-best结果数量
    bool m_showLanguage{false};     // 是否显示语言信息
    bool m_showUtterances{false};   // 是否显示话语信息
    QString m_resultType{"full"};   // 结果类型
    QString m_language{"zh-CN"};    // 语言代码
    
    // 新增参数
    int m_segDuration{100};     // 分段时长(毫秒)
    bool m_streaming{true};     // 是否启用流式传输
    
    // 状态管理
    bool m_connected{false};    // 连接状态
    QMutex m_mutex;             // 互斥锁
    QString m_logid;            // 服务器返回的logid，用于排错
    
    // WebSocket相关
    QWebSocket* m_webSocket;        // WebSocket对象
    QTimer* m_connectionTimer;      // 连接超时定时器
    
    // 回调
    AsrQtCallback* m_callback{nullptr};  // 回调对象指针
};

} // namespace Asr

#endif // ASR_QT_CLIENT_H 