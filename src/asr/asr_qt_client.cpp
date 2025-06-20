//
// ASR Qt客户端实现
// 
// 本文件实现了基于Qt的ASR（自动语音识别）WebSocket客户端
// 完全兼容火山引擎ASR WebSocket协议，支持流式音频识别
// 
// 协议特性：
// - 支持GZIP压缩
// - 支持JSON序列化
// - 支持序列号管理
// - 支持分段音频传输
// 
// 作者: PerfXAgent Team
// 版本: 1.2.0
// 日期: 2024
// 参考: 火山引擎ASR WebSocket协议文档
//

#include "asr/asr_qt_client.h"
#include <QObject>
#include <QMutexLocker>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QDebug>
#include <QUuid>
#include <QDataStream>
#include <QIODevice>
#include <QWebSocket>
#include <QTimer>
#include <QFile>
#include <QJsonValue>
#include <QAbstractSocket>
#include <zlib.h>
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <iostream>
#include <QThread>
#include <QDateTime>
#include <QMap>

namespace Asr {

// ============================================================================
// 协议常量定义 - 与官方Python示例保持一致
// ============================================================================

// 连接超时时间（毫秒）
const int CONNECTION_TIMEOUT_MS = 10000;

// 协议版本号 (4 bits)
const uint8_t PROTOCOL_VERSION = 0x01;

// 消息类型定义 (4 bits)
[[maybe_unused]] const uint8_t FULL_CLIENT_REQUEST = 0x01;    // 完整客户端请求
[[maybe_unused]] const uint8_t AUDIO_ONLY_REQUEST = 0x02;     // 仅音频请求
[[maybe_unused]] const uint8_t FULL_SERVER_RESPONSE = 0x09;   // 完整服务器响应
[[maybe_unused]] const uint8_t SERVER_ACK = 0x0B;             // 服务器确认
const uint8_t SERVER_ERROR_RESPONSE = 0x0F;                   // 服务器错误响应

// 消息类型特定标志 (4 bits)
const uint8_t POS_SEQUENCE = 0x01;        // 正序列号
const uint8_t NEG_WITH_SEQUENCE = 0x03;   // 负序列号带序列号

// 消息序列化方法 (4 bits)
const uint8_t NO_SERIALIZATION = 0x00;    // 无序列化
const uint8_t JSON_SERIALIZATION = 0x01;  // JSON序列化

// 消息压缩类型 (4 bits)
const uint8_t GZIP_COMPRESSION = 0x01;    // GZIP压缩

// ============================================================================
// AsrQtClient 类实现
// ============================================================================

/**
 * @brief 构造函数
 * @param parent 父对象指针
 * 
 * 初始化ASR客户端，设置WebSocket连接、SSL配置、信号槽连接等
 */
AsrQtClient::AsrQtClient(QObject *parent)
    : QObject(parent)
    , m_webSocket(new QWebSocket())
    , m_connectionTimer(new QTimer(this))
{
    m_uid = "test";
    
    // 强制不使用代理，避免网络问题
    m_webSocket->setProxy(QNetworkProxy::NoProxy);
    
    // 配置SSL设置 - 不校验证书，支持TLS 1.2+
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_webSocket->setSslConfiguration(sslConfig);
    
    // 设置网络选项 - 无限制缓冲区大小
    m_webSocket->setReadBufferSize(0); // 无限制读取缓冲区
    m_webSocket->setMaxAllowedIncomingMessageSize(1000000000); // 设置最大消息大小为1GB，与Python版本一致
    
    // 生成唯一的请求ID
    m_reqId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    // 连接WebSocket信号到对应的槽函数
    QObject::connect(m_webSocket, &QWebSocket::connected, this, &AsrQtClient::onWebSocketConnected);
    QObject::connect(m_webSocket, &QWebSocket::disconnected, this, &AsrQtClient::onWebSocketDisconnected);
    QObject::connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &AsrQtClient::onWebSocketError);
    QObject::connect(m_webSocket, &QWebSocket::textMessageReceived, this, &AsrQtClient::onWebSocketTextMessageReceived);
    QObject::connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &AsrQtClient::onWebSocketBinaryMessageReceived);
    
    // 设置连接超时定时器
    m_connectionTimer->setSingleShot(true);
    QObject::connect(m_connectionTimer, &QTimer::timeout, this, &AsrQtClient::onConnectionTimeout);
    
    // 连接SSL错误信号，用于调试
    QObject::connect(m_webSocket, &QWebSocket::sslErrors, this, [](const QList<QSslError> &errors){
        qWarning() << "[DEBUG] SSL Errors detected:";
        for (const QSslError &err : errors) {
            qWarning() << "[DEBUG] SSL Error:" << err.errorString();
        }
    });
    
    // 连接WebSocket状态变化信号，用于调试
    QObject::connect(m_webSocket, &QWebSocket::stateChanged, this, [this](QAbstractSocket::SocketState state){
        qDebug() << "[DEBUG] WebSocket state changed:" << state;
        qDebug() << "[DEBUG]   - Local address:" << m_webSocket->localAddress().toString();
        qDebug() << "[DEBUG]   - Local port:" << m_webSocket->localPort();
        qDebug() << "[DEBUG]   - Peer address:" << m_webSocket->peerAddress().toString();
        qDebug() << "[DEBUG]   - Peer port:" << m_webSocket->peerPort();
        qDebug() << "[DEBUG]   - Is valid:" << m_webSocket->isValid();
        qDebug() << "[DEBUG]   - Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    });
}

/**
 * @brief 析构函数
 * 
 * 断开连接并清理资源
 */
AsrQtClient::~AsrQtClient() {
    disconnect();
}

// ============================================================================
// 配置方法 - 设置客户端参数
// ============================================================================

/**
 * @brief 设置应用ID
 * @param appId 应用ID
 */
void AsrQtClient::setAppId(const QString& appId) {
    m_appId = appId;
}

/**
 * @brief 设置访问令牌
 * @param token 访问令牌
 */
void AsrQtClient::setToken(const QString& token) {
    m_token = token;
}

/**
 * @brief 设置密钥
 * @param secretKey 密钥
 */
void AsrQtClient::setSecretKey(const QString& secretKey) {
    m_secretKey = secretKey;
}

/**
 * @brief 设置认证类型
 * @param authType 认证类型
 */
void AsrQtClient::setAuthType(AuthType authType) {
    m_authType = authType;
}

/**
 * @brief 设置音频格式参数
 * @param format 音频格式 (wav, mp3, pcm等)
 * @param channels 声道数
 * @param sampleRate 采样率
 * @param bits 位深度
 */
void AsrQtClient::setAudioFormat(const QString& format, int channels, int sampleRate, int bits) {
    m_format = format;
    m_channels = channels;
    m_sampleRate = sampleRate;
    m_bits = bits;
}

/**
 * @brief 设置集群信息
 * @param cluster 集群名称
 */
void AsrQtClient::setCluster(const QString& cluster) {
    m_cluster = cluster;
}

/**
 * @brief 设置回调对象
 * @param callback 回调对象指针
 */
void AsrQtClient::setCallback(AsrQtCallback* callback) {
    m_callback = callback;
}

/**
 * @brief 设置用户ID
 * @param uid 用户ID
 */
void AsrQtClient::setUid(const QString& uid) {
    m_uid = uid;
}

/**
 * @brief 设置工作流
 * @param workflow 工作流字符串
 */
void AsrQtClient::setWorkflow(const QString& workflow) {
    m_workflow = workflow;
}

/**
 * @brief 设置序列号
 * @param seq 序列号
 */
void AsrQtClient::setSequence(int seq) {
    m_seq = seq;
}

/**
 * @brief 设置N-best结果数量
 * @param nbest N-best数量
 */
void AsrQtClient::setNbest(int nbest) {
    m_nbest = nbest;
}

/**
 * @brief 设置是否显示话语信息
 * @param show 是否显示
 */
void AsrQtClient::setShowUtterances(bool show) {
    m_showUtterances = show;
}

/**
 * @brief 设置是否显示语言信息
 * @param show 是否显示
 */
void AsrQtClient::setShowLanguage(bool show) {
    m_showLanguage = show;
}

/**
 * @brief 设置结果类型
 * @param resultType 结果类型
 */
void AsrQtClient::setResultType(const QString& resultType) {
    m_resultType = resultType;
}

/**
 * @brief 设置语言
 * @param language 语言代码
 */
void AsrQtClient::setLanguage(const QString& language) {
    m_language = language;
}

/**
 * @brief 设置分段时长
 * @param duration 分段时长(毫秒)
 */
void AsrQtClient::setSegDuration(int duration) {
    m_segDuration = duration;
}

/**
 * @brief 设置是否启用流式传输
 * @param streaming 是否启用流式传输
 */
void AsrQtClient::setStreaming(bool streaming) {
    m_streaming = streaming;
}

// ============================================================================
// 连接控制方法
// ============================================================================

/**
 * @brief 连接到ASR服务器
 * @return 是否成功启动连接
 * 
 * 建立WebSocket连接到ASR服务器，设置认证头部，启动连接超时定时器
 */
bool AsrQtClient::connect() {
    if (m_connected) {
        qDebug() << "ASR client is already connected";
        return true;
    }
    
    // 获取凭据
    Credentials creds = getCredentialsFromEnv();
    if (!creds.isValid) {
        qWarning() << "Invalid credentials";
        return false;
    }
    
    // 设置凭据
    m_appId = creds.appId;
    m_token = creds.accessToken;
    m_secretKey = creds.secretKey;
    
    // 构建WebSocket URL
    QUrl url(m_url);
    if (m_authType == TOKEN) {
        QUrlQuery query(url);
        query.addQueryItem("app_id", m_appId);
        query.addQueryItem("token", m_token);
        url.setQuery(query);
    }
    
    qDebug() << "=== WebSocket连接调试信息 ===";
    qDebug() << "目标URL:" << url.toString();
    qDebug() << "App ID:" << m_appId;
    qDebug() << "Token:" << (m_token.length() > 8 ? m_token.left(4) + "****" + m_token.right(4) : "****");
    qDebug() << "Secret Key:" << (m_secretKey.length() > 8 ? m_secretKey.left(4) + "****" + m_secretKey.right(4) : "****");
    
    // 构建HTTP请求头
    QNetworkRequest request(url);
    
    // 设置必需的WebSocket头部
    request.setRawHeader("Upgrade", "websocket");
    request.setRawHeader("Connection", "upgrade");
    
    // 设置ASR认证头部
    request.setRawHeader("X-Api-Resource-Id", "volc.bigasr.sauc.duration");
    request.setRawHeader("X-Api-Access-Key", m_token.toUtf8());
    request.setRawHeader("X-Api-App-Key", m_appId.toUtf8());
    request.setRawHeader("X-Api-Request-Id", m_reqId.toUtf8());
    
    // 打印发送的Header信息
    qDebug() << "=== 发送的HTTP Header ===";
    qDebug() << "Upgrade:" << request.rawHeader("Upgrade");
    qDebug() << "Connection:" << request.rawHeader("Connection");
    qDebug() << "X-Api-Resource-Id:" << request.rawHeader("X-Api-Resource-Id");
    qDebug() << "X-Api-Access-Key:" << request.rawHeader("X-Api-Access-Key");
    qDebug() << "X-Api-App-Key:" << request.rawHeader("X-Api-App-Key");
    qDebug() << "X-Api-Request-Id:" << request.rawHeader("X-Api-Request-Id");
    
    // 设置WebSocket请求
    m_webSocket->open(request);
    
    // 启动连接超时定时器
    m_connectionTimer->start(CONNECTION_TIMEOUT_MS);
    
    qDebug() << "=== WebSocket连接状态跟踪 ===";
    qDebug() << "连接前状态:" << m_webSocket->state();
    qDebug() << "连接前是否有效:" << m_webSocket->isValid();
    qDebug() << "连接前错误:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    return true;
}

/**
 * @brief 断开连接
 * 
 * 关闭WebSocket连接，停止定时器，重置连接状态
 */
void AsrQtClient::disconnect() {
    QMutexLocker locker(&m_mutex);
    
    if (m_webSocket) {
        m_webSocket->close();
    }
    
    m_connectionTimer->stop();
    m_connected = false;
}

/**
 * @brief 检查是否已连接
 * @return 是否已连接
 */
bool AsrQtClient::isConnected() const {
    return m_connected;
}

// ============================================================================
// 协议消息生成方法
// ============================================================================

/**
 * @brief 生成协议头部
 * @param messageType 消息类型
 * @param messageTypeSpecificFlags 消息类型特定标志
 * @param serialMethod 序列化方法
 * @param compressionType 压缩类型
 * @param reservedData 保留数据
 * @return 4字节的协议头部
 * 
 * 按照火山引擎ASR协议生成消息头部：
 * Byte 0: protocol_version(4 bits), header_size(4 bits)
 * Byte 1: message_type(4 bits), message_type_specific_flags(4 bits)
 * Byte 2: serialization_method(4 bits), message_compression(4 bits)
 * Byte 3: reserved (8 bits)
 */
QByteArray AsrQtClient::generateHeader(uint8_t messageType, uint8_t messageTypeSpecificFlags, 
                                      uint8_t serialMethod, uint8_t compressionType, uint8_t reservedData) {
    QByteArray header;
    uint8_t headerSize = 1;
    
    // protocol_version(4 bits), header_size(4 bits)
    header.append((PROTOCOL_VERSION << 4) | headerSize);
    
    // message_type(4 bits), message_type_specific_flags(4 bits)
    header.append((messageType << 4) | messageTypeSpecificFlags);
    
    // serialization_method(4 bits), message_compression(4 bits)
    header.append((serialMethod << 4) | compressionType);
    
    // reserved (8 bits)
    header.append(reservedData);
    
    return header;
}

/**
 * @brief 生成序列号前的内容
 * @param sequence 序列号
 * @return 4字节的序列号数据（大端序，有符号）
 * 
 * 将序列号转换为4字节大端序有符号整数
 */
QByteArray AsrQtClient::generateBeforePayload(int32_t sequence) {
    QByteArray beforePayload;
    QDataStream stream(&beforePayload, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << sequence; // 4字节，大端序，有符号
    return beforePayload;
}

// ============================================================================
// 音频数据发送方法
// ============================================================================

/**
 * @brief 发送音频数据
 * @param audioData 音频数据
 * @param isLast 是否为最后一个包
 * @return 是否发送成功
 * 
 * 将音频数据压缩后按照协议格式发送
 */
bool AsrQtClient::sendAudio(const QByteArray& audioData, bool isLast) {
    if (!m_connected) {
        qWarning() << "ASR client is not connected";
        return false;
    }
    
    // 压缩音频数据
    QByteArray compressedAudio = gzipCompress(audioData);
    
    // 确定消息类型和标志
    uint8_t messageType = AUDIO_ONLY_REQUEST;
    uint8_t messageFlags = isLast ? NEG_WITH_SEQUENCE : POS_SEQUENCE;
    
    // 生成头部
    QByteArray header = generateHeader(messageType, messageFlags, JSON_SERIALIZATION, GZIP_COMPRESSION, 0x00);
    
    // 生成序列号前的内容
    int32_t currentSeq = isLast ? -m_seq : m_seq;
    QByteArray beforePayload = generateBeforePayload(currentSeq);
    
    // 生成payload大小
    QByteArray payloadSize;
    QDataStream sizeStream(&payloadSize, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << static_cast<uint32_t>(compressedAudio.size());
    
    // 组装完整消息
    QByteArray message = header + beforePayload + payloadSize + compressedAudio;
    
    qint64 bytesSent = m_webSocket->sendBinaryMessage(message);
    if (bytesSent == -1) {
        qWarning() << "Failed to send audio data";
        return false;
    }
    
    qDebug() << "Audio data sent, seq:" << currentSeq << "size:" << audioData.size() << "bytes, isLast:" << isLast;
    
    if (!isLast) {
        m_seq++;
    }
    
    return true;
}

/**
 * @brief 发送音频文件
 * @param filePath 音频文件路径
 * @return 是否发送成功
 * 
 * 读取整个音频文件并发送
 */
bool AsrQtClient::sendAudioFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open audio file:" << filePath;
        return false;
    }
    
    QByteArray audioData = file.readAll();
    file.close();
    
    return sendAudio(audioData, true);
}

// ============================================================================
// 完整客户端请求方法
// ============================================================================

/**
 * @brief 发送完整客户端请求并等待响应
 * @param timeoutMs 超时时间（毫秒）
 * @param response 响应字符串指针
 * @return 是否成功
 * 
 * 发送初始参数请求并等待服务器响应
 */
bool AsrQtClient::sendFullClientRequestAndWaitResponse(int timeoutMs, QString* response) {
    if (!m_connected) {
        qWarning() << "ASR client is not connected";
        return false;
    }
    
    qDebug() << "=== 发送完整客户端请求 ===";
    qDebug() << "发送时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    
    // 检查连接状态
    qDebug() << "发送前连接状态:";
    qDebug() << "  - 状态:" << m_webSocket->state();
    qDebug() << "  - 是否有效:" << m_webSocket->isValid();
    qDebug() << "  - 错误:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    qDebug() << "  - 本地地址:" << m_webSocket->localAddress().toString() << ":" << m_webSocket->localPort();
    qDebug() << "  - 远程地址:" << m_webSocket->peerAddress().toString() << ":" << m_webSocket->peerPort();
    
    // 获取JSON字符串
    QString jsonStr = getFullClientRequestJson();
    
    // 确保使用UTF-8编码，与Python的str.encode()一致
    QByteArray jsonData = jsonStr.toUtf8();
    qDebug() << "JSON编码检查:";
    qDebug() << "  - JSON字符串长度:" << jsonStr.length();
    qDebug() << "  - UTF-8字节长度:" << jsonData.size();
    qDebug() << "  - 编码:" << jsonData.toHex();
    
    // 压缩JSON数据
    QByteArray compressedData = gzipCompress(jsonData);
    
    // 生成头部
    QByteArray header = generateHeader(FULL_CLIENT_REQUEST, POS_SEQUENCE, JSON_SERIALIZATION, GZIP_COMPRESSION, 0x00);
    
    // 生成序列号前的内容
    QByteArray beforePayload = generateBeforePayload(1);
    
    // 生成payload大小
    QByteArray payloadSize;
    QDataStream sizeStream(&payloadSize, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << static_cast<uint32_t>(compressedData.size());
    
    // 组装完整消息
    QByteArray message = header + beforePayload + payloadSize + compressedData;
    
    // 打印详细的协议信息
    qDebug() << "协议详情:";
    qDebug() << "HEADER:" << header.toHex(' ').toUpper();
    qDebug() << "SEQ:" << beforePayload.toHex(' ').toUpper();
    qDebug() << "PAYLOAD_LEN:" << payloadSize.toHex(' ').toUpper();
    qDebug() << "PAYLOAD_HEAD:" << compressedData.left(20).toHex(' ').toUpper();
    qDebug() << "FULL_CLIENT_REQUEST_WITH_PAYLOAD_HEX:" << message.toHex().toUpper();
    
    // 发送消息
    qint64 bytesSent = m_webSocket->sendBinaryMessage(message);
    qDebug() << "发送结果:";
    qDebug() << "  - 发送字节数:" << bytesSent;
    qDebug() << "  - 预期字节数:" << message.size();
    qDebug() << "  - 发送是否成功:" << (bytesSent == message.size() ? "是" : "否");
    
    if (bytesSent == -1) {
        qWarning() << "发送失败，错误:" << m_webSocket->error() << "-" << m_webSocket->errorString();
        return false;
    }
    
    // 检查发送后的连接状态
    qDebug() << "发送后连接状态:";
    qDebug() << "  - 状态:" << m_webSocket->state();
    qDebug() << "  - 是否有效:" << m_webSocket->isValid();
    qDebug() << "  - 错误:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    // 等待响应
    return waitForResponse(timeoutMs, response);
}

/**
 * @brief 发送完整客户端请求并获取响应
 * @param timeoutMs 超时时间（毫秒）
 * @return 响应字符串
 * 
 * 发送初始参数请求并返回服务器响应
 */
QString AsrQtClient::sendFullClientRequestAndGetResponse(int timeoutMs) {
    if (!m_connected) {
        qWarning() << "ASR client is not connected";
        return QString();
    }
    
    // 发送请求
    if (!sendParams()) {
        return QString();
    }
    
    // 等待响应
    QString response;
    if (waitForResponse(timeoutMs, &response)) {
        return response;
    }
    return QString();
}

/**
 * @brief 获取完整客户端请求的JSON字符串
 * @return JSON格式的请求字符串
 * 
 * 构建符合火山引擎ASR协议的请求JSON
 */
QString AsrQtClient::getFullClientRequestJson() const {
    // 使用与Python完全一致的JSON字符串，包含空格
    // Python输出: {"user":{"uid":"test"},"audio":{"format":"wav","sample_rate":16000,"bits":16,"channel":1,"codec":"raw"},"request":{"model_name":"bigmodel","enable_punc":true}}
    // 注意：Python的json.dumps()默认包含空格，所以这里也要包含空格
    QString jsonStr = R"({"user": {"uid": "test"}, "audio": {"format": "wav", "sample_rate": 16000, "bits": 16, "channel": 1, "codec": "raw"}, "request": {"model_name": "bigmodel", "enable_punc": true}})";
    
    // 添加调试信息：打印JSON的字节表示
    QByteArray jsonBytes = jsonStr.toUtf8();
    QString jsonHex;
    for (int i = 0; i < jsonBytes.size(); ++i) {
        jsonHex += QString("%1 ").arg(static_cast<unsigned char>(jsonBytes[i]), 2, 16, QChar('0'));
    }
    qDebug() << "JSON bytes (hex):" << jsonHex.trimmed();
    qDebug() << "JSON length:" << jsonBytes.size() << "bytes";
    
    return jsonStr;
}

// ============================================================================
// WebSocket事件处理槽函数
// ============================================================================

/**
 * @brief WebSocket连接建立事件
 * 
 * 连接成功时调用，设置连接状态，触发回调
 */
void AsrQtClient::onWebSocketConnected() {
    QMutexLocker locker(&m_mutex);
    
    m_connectionTimer->stop();
    m_connected = true;
    
    qDebug() << "=== WebSocket连接成功 ===";
    qDebug() << "连接时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "本地地址:" << m_webSocket->localAddress().toString() << ":" << m_webSocket->localPort();
    qDebug() << "远程地址:" << m_webSocket->peerAddress().toString() << ":" << m_webSocket->peerPort();
    qDebug() << "连接状态:" << m_webSocket->state();
    qDebug() << "是否有效:" << m_webSocket->isValid();
    qDebug() << "错误状态:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    // 尝试获取响应头信息（如果可用）
    qDebug() << "=== 尝试获取响应头信息 ===";
    // 注意：QWebSocket可能不直接提供HTTP响应头访问
    // 但我们可以通过其他方式获取logid等信息
    
    if (m_callback) {
        m_callback->onOpen(this);
    }
    
    emit connected();
}

/**
 * @brief WebSocket连接断开事件
 * 
 * 连接断开时调用，重置连接状态，触发回调
 */
void AsrQtClient::onWebSocketDisconnected() {
    QMutexLocker locker(&m_mutex);
    
    m_connectionTimer->stop();
    m_connected = false;
    
    qDebug() << "=== WebSocket连接断开 ===";
    qDebug() << "断开时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "断开前状态:" << m_webSocket->state();
    qDebug() << "断开原因:" << m_webSocket->closeReason();
    qDebug() << "断开代码:" << m_webSocket->closeCode();
    qDebug() << "错误状态:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    if (m_callback) {
        m_callback->onClose(this);
    }
    
    emit disconnected();
}

/**
 * @brief WebSocket错误事件
 * @param error 错误类型
 * 
 * 发生错误时调用，记录错误信息，触发回调
 */
void AsrQtClient::onWebSocketError(QAbstractSocket::SocketError error) {
    QMutexLocker locker(&m_mutex);
    
    m_connectionTimer->stop();
    
    QString errorMsg = m_webSocket->errorString();
    
    qDebug() << "=== WebSocket错误 ===";
    qDebug() << "错误时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "错误代码:" << error;
    qDebug() << "错误描述:" << errorMsg;
    qDebug() << "当前状态:" << m_webSocket->state();
    qDebug() << "本地地址:" << m_webSocket->localAddress().toString() << ":" << m_webSocket->localPort();
    qDebug() << "远程地址:" << m_webSocket->peerAddress().toString() << ":" << m_webSocket->peerPort();
    qDebug() << "目标URL:" << m_url;
    
    // 根据错误类型提供建议
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
            qDebug() << "建议: 检查服务器地址和端口是否正确";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            qDebug() << "建议: 服务器主动断开连接，检查认证信息或请求格式";
            break;
        case QAbstractSocket::HostNotFoundError:
            qDebug() << "建议: 检查网络连接和DNS解析";
            break;
        case QAbstractSocket::SocketTimeoutError:
            qDebug() << "建议: 检查网络连接和防火墙设置";
            break;
        case QAbstractSocket::SslHandshakeFailedError:
            qDebug() << "建议: 检查SSL/TLS配置和证书";
            break;
        default:
            qDebug() << "建议: 检查网络连接和服务器状态";
            break;
    }
    
    if (m_callback) {
        m_callback->onError(this, errorMsg);
    }
    
    emit errorOccurred(errorMsg);
}

/**
 * @brief WebSocket文本消息接收事件
 * @param message 接收到的文本消息
 * 
 * 接收到文本消息时调用，直接转发给回调
 */
void AsrQtClient::onWebSocketTextMessageReceived(const QString& message) {
    qDebug() << "=== 收到文本消息 ===";
    qDebug() << "接收时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "消息内容:" << message;
    
    if (m_callback) {
        m_callback->onMessage(this, message);
    }
    
    emit messageReceived(message);
}

/**
 * @brief WebSocket二进制消息接收事件
 * @param message 接收到的二进制消息
 * 
 * 接收到二进制消息时调用，解析协议格式并转发给回调
 */
void AsrQtClient::onWebSocketBinaryMessageReceived(const QByteArray& message) {
    qDebug() << "=== 收到二进制消息 ===";
    qDebug() << "接收时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "消息大小:" << message.size() << "字节";
    
    // 添加原始数据的十六进制转储（前100字节）
    QString hexDump;
    int dumpSize = qMin(100, message.size());
    for (int i = 0; i < dumpSize; ++i) {
        hexDump += QString("%1 ").arg(static_cast<unsigned char>(message[i]), 2, 16, QChar('0'));
    }
    if (message.size() > dumpSize) {
        hexDump += "...";
    }
    qDebug() << "原始数据(前100字节):" << hexDump.trimmed();
    
    // 解析响应
    QVariantMap result = parseResponse(message);
    
    // 提取logid（如果存在）
    if (result.contains("payload_msg")) {
        QVariantMap payloadMsg = result["payload_msg"].toMap();
        if (payloadMsg.contains("result")) {
            QVariantMap resultMap = payloadMsg["result"].toMap();
            if (resultMap.contains("additions")) {
                QVariantMap additions = resultMap["additions"].toMap();
                if (additions.contains("log_id")) {
                    QString logId = additions["log_id"].toString();
                    m_logid = logId;
                    qDebug() << "提取到X-Tt-Logid:" << logId;
                }
            }
        }
    }
    
    // 转换为JSON字符串
    QJsonDocument doc = QJsonDocument::fromVariant(result);
    QString messageStr = doc.toJson(QJsonDocument::Compact);
    
    qDebug() << "解析后的响应:" << messageStr;
    
    // 发送信号
    emit messageReceived(messageStr);
    
    if (m_callback) {
        m_callback->onMessage(this, messageStr);
    }
}

/**
 * @brief 连接超时事件
 * 
 * 连接超时时调用，触发错误回调
 */
void AsrQtClient::onConnectionTimeout() {
    QMutexLocker locker(&m_mutex);
    
    qWarning() << "Connection timeout";
    
    if (m_callback) {
        m_callback->onError(this, "Connection timeout");
    }
    
    emit errorOccurred("Connection timeout");
}

// ============================================================================
// 内部协议处理方法
// ============================================================================

/**
 * @brief 发送初始参数
 * @return 是否发送成功
 * 
 * 构建并发送完整的客户端请求，包含音频格式、用户信息等参数
 */
bool AsrQtClient::sendParams() {
    qDebug() << "Sending initial parameters";
    
    // 检查连接状态
    qDebug() << "Connection status before sending:";
    qDebug() << "  - State:" << m_webSocket->state();
    qDebug() << "  - Is valid:" << m_webSocket->isValid();
    qDebug() << "  - Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    // 添加小延迟，确保连接完全建立
    QThread::msleep(500);
    
    // 构建JSON请求
    QString jsonStr = getFullClientRequestJson();
    QByteArray jsonData = jsonStr.toUtf8();
    
    // 压缩JSON数据
    QByteArray compressedData = gzipCompress(jsonData);
    
    // 生成头部 - 使用FULL_CLIENT_REQUEST (0x01) 而不是 AUDIO_ONLY_REQUEST (0x02)
    QByteArray header = generateHeader(FULL_CLIENT_REQUEST, POS_SEQUENCE, JSON_SERIALIZATION, GZIP_COMPRESSION, 0x00);
    
    // 生成序列号前的内容
    QByteArray beforePayload = generateBeforePayload(m_seq);
    
    // 生成payload大小
    QByteArray payloadSize;
    QDataStream sizeStream(&payloadSize, QIODevice::WriteOnly);
    sizeStream.setByteOrder(QDataStream::BigEndian);
    sizeStream << static_cast<uint32_t>(compressedData.size());
    
    // 组装完整消息
    QByteArray fullRequest = header + beforePayload + payloadSize + compressedData;

    // 打印协议头部、序列号、payload长度、payload前20字节、完整包hex
    qDebug() << "HEADER:" << header.toHex(' ').toUpper();
    qDebug() << "SEQ:" << beforePayload.toHex(' ').toUpper();
    qDebug() << "PAYLOAD_LEN:" << payloadSize.toHex(' ').toUpper();
    qDebug() << "PAYLOAD_HEAD:" << compressedData.left(20).toHex(' ').toUpper();
    qDebug() << "FULL_CLIENT_REQUEST_WITH_PAYLOAD_HEX:" << fullRequest.toHex().toUpper();
    
    qint64 bytesSent = m_webSocket->sendBinaryMessage(fullRequest);
    if (bytesSent == -1) {
        qWarning() << "Failed to send binary message";
        return false;
    }
    
    qDebug() << "Bytes sent:" << bytesSent;
    
    // 发送后立即检查连接状态
    qDebug() << "Connection status after sending:";
    qDebug() << "  - State:" << m_webSocket->state();
    qDebug() << "  - Is valid:" << m_webSocket->isValid();
    qDebug() << "  - Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    
    return true;
}

/**
 * @brief 等待响应
 * @param timeoutMs 超时时间（毫秒）
 * @param response 响应字符串指针
 * @return 是否成功接收到响应
 * 
 * 使用事件循环等待服务器响应，支持超时机制
 */
bool AsrQtClient::waitForResponse(int timeoutMs, QString* response) {
    qDebug() << "=== 等待服务器响应 ===";
    qDebug() << "等待开始时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << "超时时间:" << timeoutMs << "毫秒";
    
    QEventLoop loop;
    QString receivedResponse;
    bool responseReceived = false;
    
    // 连接消息接收信号
    QObject::connect(this, &AsrQtClient::messageReceived, &loop, [&](const QString& msg) {
        qDebug() << "收到响应消息:" << msg;
        receivedResponse = msg;
        responseReceived = true;
        loop.quit();
    });
    
    // 连接错误信号
    QObject::connect(this, &AsrQtClient::errorOccurred, &loop, [&](const QString& error) {
        qDebug() << "等待响应时发生错误:" << error;
        loop.quit();
    });
    
    // 设置超时定时器
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        qDebug() << "响应等待超时";
        qDebug() << "超时时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        qDebug() << "当前连接状态:" << m_webSocket->state();
        qDebug() << "当前错误状态:" << m_webSocket->error() << "-" << m_webSocket->errorString();
        loop.quit();
    });
    
    timer.start(timeoutMs);
    
    // 等待响应
    loop.exec();
    
    // 断开所有信号连接
    QObject::disconnect(this, &AsrQtClient::messageReceived, &loop, nullptr);
    QObject::disconnect(this, &AsrQtClient::errorOccurred, &loop, nullptr);
    QObject::disconnect(&timer, &QTimer::timeout, &loop, nullptr);
    
    if (responseReceived) {
        qDebug() << "成功收到响应";
        if (response) {
            *response = receivedResponse;
        }
        return true;
    } else {
        qWarning() << "在超时时间内未收到响应";
        qDebug() << "等待结束时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        return false;
    }
}

/**
 * @brief 解析服务器响应
 * @param response 原始响应数据
 * @return 解析后的响应映射
 * 
 * 按照火山引擎ASR协议解析二进制响应数据
 */
QVariantMap AsrQtClient::parseResponse(const QByteArray& response) {
    QVariantMap result;
    result["is_last_package"] = false;
    
    if (response.size() < 4) {
        qWarning() << "Response too short";
        return result;
    }
    
    // 解析头部
    uint8_t protocolVersion = response[0] >> 4;
    uint8_t headerSize = response[0] & 0x0f;
    uint8_t messageType = response[1] >> 4;
    uint8_t messageTypeSpecificFlags = response[1] & 0x0f;
    uint8_t serializationMethod = response[2] >> 4;
    uint8_t messageCompression = response[2] & 0x0f;
    uint8_t reserved = response[3];
    
    qDebug() << "Response header - protocol:" << protocolVersion 
             << "headerSize:" << headerSize
             << "type:" << messageType 
             << "flags:" << messageTypeSpecificFlags
             << "serial:" << serializationMethod 
             << "compress:" << messageCompression
             << "reserved:" << reserved;
    
    // 计算头部扩展和payload位置
    uint32_t headerExtensionsSize = headerSize * 4;
    QByteArray headerExtensions = response.mid(4, headerExtensionsSize - 4);
    QByteArray payload = response.mid(headerExtensionsSize);
    
    // 处理序列号
    if (messageTypeSpecificFlags & 0x01) {
        if (payload.size() >= 4) {
            QDataStream seqStream(payload.left(4));
            seqStream.setByteOrder(QDataStream::BigEndian);
            int32_t seq;
            seqStream >> seq;
            result["payload_sequence"] = seq;
            payload = payload.mid(4);
        }
    }
    
    // 处理最后包标志
    if (messageTypeSpecificFlags & 0x02) {
        result["is_last_package"] = true;
    }
    
    QByteArray payloadMsg;
    uint32_t payloadSize = 0;
    
    // 根据消息类型处理payload
    if (messageType == AUDIO_ONLY_REQUEST) {
        if (payload.size() >= 4) {
            QDataStream sizeStream(payload.left(4));
            sizeStream.setByteOrder(QDataStream::BigEndian);
            sizeStream >> payloadSize;
            payloadMsg = payload.mid(4);
        }
    } else if (messageType == SERVER_ERROR_RESPONSE) {
        if (payload.size() >= 8) {
            QDataStream codeStream(payload.left(4));
            codeStream.setByteOrder(QDataStream::BigEndian);
            uint32_t code;
            codeStream >> code;
            result["code"] = code;
            
            QDataStream sizeStream(payload.mid(4, 4));
            sizeStream.setByteOrder(QDataStream::BigEndian);
            sizeStream >> payloadSize;
            payloadMsg = payload.mid(8);
        }
    }
    
    // 处理payload内容
    if (!payloadMsg.isEmpty()) {
        // 解压缩
        if (messageCompression == GZIP_COMPRESSION) {
            payloadMsg = gzipDecompress(payloadMsg);
        }
        
        // 反序列化
        if (serializationMethod == JSON_SERIALIZATION) {
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(payloadMsg, &parseError);
            if (parseError.error == QJsonParseError::NoError) {
                result["payload_msg"] = doc.toVariant();
            } else {
                result["payload_msg"] = QString::fromUtf8(payloadMsg);
            }
        } else if (serializationMethod != NO_SERIALIZATION) {
            result["payload_msg"] = QString::fromUtf8(payloadMsg);
        }
        
        result["payload_size"] = payloadSize;
    }
    
    return result;
}

// ============================================================================
// 工具方法 - 压缩和解压缩
// ============================================================================

/**
 * @brief GZIP压缩
 * @param data 原始数据
 * @return 压缩后的数据
 * 
 * 使用zlib库进行GZIP压缩
 */
QByteArray AsrQtClient::gzipCompress(const QByteArray& data) {
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // 使用与Python gzip.compress()完全一致的参数
    // 31 = 16 + 15: 16表示gzip格式，15表示最大压缩级别
    // 8: 默认内存级别
    // Z_DEFAULT_STRATEGY: 默认策略
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        qWarning() << "Failed to initialize zlib for compression";
        return data;
    }
    
    strm.avail_in = data.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    
    QByteArray compressed;
    char outBuffer[4096];
    
    do {
        strm.avail_out = sizeof(outBuffer);
        strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
        
        int ret = deflate(&strm, Z_FINISH);
        if (ret == Z_STREAM_ERROR) {
            qWarning() << "zlib compression error";
            deflateEnd(&strm);
            return data;
        }
        
        int have = sizeof(outBuffer) - strm.avail_out;
        compressed.append(outBuffer, have);
    } while (strm.avail_out == 0);
    
    deflateEnd(&strm);
    
    // 修复gzip头部，使其与Python一致
    if (compressed.size() >= 10) {
        // 使用当前时间戳，与Python的gzip.compress()一致
        QDateTime currentTime = QDateTime::currentDateTime();
        uint32_t timestamp = currentTime.toSecsSinceEpoch();
        
        // 写入时间戳（小端序）
        compressed[4] = (timestamp >> 0) & 0xFF;
        compressed[5] = (timestamp >> 8) & 0xFF;
        compressed[6] = (timestamp >> 16) & 0xFF;
        compressed[7] = (timestamp >> 24) & 0xFF;
        
        // 修复XFL字段，使其与Python一致: 0x02 (best compression)
        compressed[8] = 0x02;
        
        // 修复OS字段，使其与Python一致: 0xff (unknown)
        compressed[9] = 0xff;
    }
    
    // 添加调试信息：打印压缩结果的hex
    QString compressedHex;
    for (int i = 0; i < compressed.size(); ++i) {
        compressedHex += QString("%1 ").arg(static_cast<unsigned char>(compressed[i]), 2, 16, QChar('0'));
    }
    qDebug() << "Compressed data (hex):" << compressedHex.trimmed();
    qDebug() << "Compressed length:" << compressed.size() << "bytes";
    
    return compressed;
}

/**
 * @brief GZIP解压缩
 * @param data 压缩数据
 * @return 解压缩后的数据
 * 
 * 使用zlib库进行GZIP解压缩
 */
QByteArray AsrQtClient::gzipDecompress(const QByteArray& data) {
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    
    if (inflateInit2(&strm, 47) != Z_OK) {
        qWarning() << "Failed to initialize zlib for decompression";
        return data;
    }
    
    strm.avail_in = data.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    
    QByteArray decompressed;
    char outBuffer[4096];
    
    do {
        strm.avail_out = sizeof(outBuffer);
        strm.next_out = reinterpret_cast<Bytef*>(outBuffer);
        
        int ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) {
            qWarning() << "zlib decompression error";
            inflateEnd(&strm);
            return data;
        }
        
        int have = sizeof(outBuffer) - strm.avail_out;
        decompressed.append(outBuffer, have);
    } while (strm.avail_out == 0);
    
    inflateEnd(&strm);
    return decompressed;
}

// ============================================================================
// 静态工具方法
// ============================================================================

/**
 * @brief 从环境变量获取凭据
 * @return 凭据结构体
 * 
 * 从环境变量读取ASR认证信息，如果未设置则使用默认值
 */
AsrQtClient::Credentials AsrQtClient::getCredentialsFromEnv() {
    Credentials creds;
    QString envAppId = qgetenv("ASR_APP_ID");
    QString envToken = qgetenv("ASR_ACCESS_TOKEN");
    QString envSecret = qgetenv("ASR_SECRET_KEY");
    if (!envAppId.isEmpty() && !envToken.isEmpty() && !envSecret.isEmpty()) {
        std::cout << "🔐 使用环境变量中的凭据" << std::endl;
        creds.appId = envAppId;
        creds.accessToken = envToken;
        creds.secretKey = envSecret;
        creds.isValid = true;
    } else {
        std::cout << "⚠️  环境变量未设置，使用默认凭据（仅用于测试）" << std::endl;
        std::cout << "   建议设置环境变量：" << std::endl;
        std::cout << "   export ASR_APP_ID=your_app_id" << std::endl;
        std::cout << "   export ASR_ACCESS_TOKEN=your_access_token" << std::endl;
        std::cout << "   export ASR_SECRET_KEY=your_secret_key" << std::endl;
        creds.appId = "8388344882";
        creds.accessToken = "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3";
        creds.secretKey = "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e";
        creds.isValid = true;
    }
    // 脱敏显示
    QString maskedToken = creds.accessToken;
    if (maskedToken.length() > 8) {
        maskedToken = maskedToken.left(4) + "****" + maskedToken.right(4);
    }
    std::cout << "📋 凭据信息:" << std::endl;
    std::cout << "   - App ID: " << creds.appId.toStdString() << std::endl;
    std::cout << "   - Access Token: " << maskedToken.toStdString() << std::endl;
    std::cout << "   - Secret Key: " << (creds.secretKey.length() > 8 ? (creds.secretKey.left(4) + "****" + creds.secretKey.right(4)).toStdString() : "****") << std::endl;
    return creds;
}

} // namespace Asr 