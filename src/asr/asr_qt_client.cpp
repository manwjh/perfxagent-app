//
// 纯Qt版本的ASR客户端实现
// 完全兼容火山ASR WebSocket协议，无外部依赖
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

namespace Asr {

AsrQtClient::AsrQtClient(QObject *parent)
    : QObject(parent)
{
    m_webSocket = new QWebSocket();
    m_webSocket->setParent(this);
    // 强制不使用代理
    m_webSocket->setProxy(QNetworkProxy::NoProxy);
    // 设置SSL配置为不校验证书
    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
    m_webSocket->setSslConfiguration(sslConfig);
    // 设置更多网络选项
    m_webSocket->setReadBufferSize(0); // 无限制
    m_webSocket->setMaxAllowedIncomingMessageSize(0); // 无限制
    m_connectionTimer = new QTimer(this);
    // 生成请求ID
    m_reqId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    // 连接WebSocket信号
    QObject::connect(m_webSocket, &QWebSocket::connected, this, &AsrQtClient::onWebSocketConnected);
    QObject::connect(m_webSocket, &QWebSocket::disconnected, this, &AsrQtClient::onWebSocketDisconnected);
    QObject::connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
            this, &AsrQtClient::onWebSocketError);
    QObject::connect(m_webSocket, &QWebSocket::textMessageReceived, this, &AsrQtClient::onWebSocketTextMessageReceived);
    QObject::connect(m_webSocket, &QWebSocket::binaryMessageReceived, this, &AsrQtClient::onWebSocketBinaryMessageReceived);
    m_connectionTimer->setSingleShot(true);
    QObject::connect(m_connectionTimer, &QTimer::timeout, this, &AsrQtClient::onConnectionTimeout);
    // 新增：SSL错误和状态变化日志
    QObject::connect(m_webSocket, &QWebSocket::sslErrors, this, [](const QList<QSslError> &errors){
        for (const QSslError &err : errors) {
            qWarning() << "SSL Error:" << err.errorString();
        }
    });
    QObject::connect(m_webSocket, &QWebSocket::stateChanged, this, [this](QAbstractSocket::SocketState state){
        qDebug() << "WebSocket state changed:" << state;
        // 获取当前 WebSocket 实例的详细信息
        qDebug() << "  - Local address:" << m_webSocket->localAddress().toString();
        qDebug() << "  - Local port:" << m_webSocket->localPort();
        qDebug() << "  - Peer address:" << m_webSocket->peerAddress().toString();
        qDebug() << "  - Peer port:" << m_webSocket->peerPort();
        qDebug() << "  - Is valid:" << m_webSocket->isValid();
        qDebug() << "  - Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    });
}

AsrQtClient::~AsrQtClient() {
    disconnect();
}

void AsrQtClient::setAppId(const QString& appId) {
    m_appId = appId;
}

void AsrQtClient::setToken(const QString& token) {
    m_token = token;
}

void AsrQtClient::setSecretKey(const QString& secretKey) {
    m_secretKey = secretKey;
}

void AsrQtClient::setAuthType(AuthType authType) {
    m_authType = authType;
}

void AsrQtClient::setAudioFormat(const QString& format, int channels, int sampleRate, int bits) {
    m_format = format;
    m_channels = channels;
    m_sampleRate = sampleRate;
    m_bits = bits;
}

void AsrQtClient::setCluster(const QString& cluster) {
    m_cluster = cluster;
}

void AsrQtClient::setCallback(AsrQtCallback* callback) {
    m_callback = callback;
}

bool AsrQtClient::connect() {
    QMutexLocker locker(&m_mutex);
    
    if (m_connected) {
        qWarning() << "ASR client is already connected";
        return false;
    }
    
    // 打印 WebSocket 连接前的 URL 和 header
    qDebug() << "Connecting to ASR server:" << m_url;
    
    QUrl url(m_url);
    if (!m_cluster.isEmpty()) {
        QUrlQuery query(url);
        query.addQueryItem("cluster", m_cluster);
        url.setQuery(query);
    }

    qDebug() << "Final URL:" << url.toString();

    // 构造带 Authorization Header 的请求
    QNetworkRequest request(url);
    if (!m_token.isEmpty()) {
        request.setRawHeader("Host", "openspeech.bytedance.com");   //        // 强制设置 Host 头
        request.setRawHeader("Accept", "*/*");
        request.setRawHeader("User-Agent", "curl/7.54.0");
        request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    }

    // 打印 WebSocket 请求的所有 header
    qDebug() << "[WS] URL:" << request.url().toString();
    for (const QByteArray& header : request.rawHeaderList()) {
        qDebug() << "[WS] Header:" << header << ":" << request.rawHeader(header);
    }

    // 启动连接超时定时器（10秒）
    m_connectionTimer->start(10000);

    // 使用带 header 的 open 方法（你需要的是 QWebSocket::open(QNetworkRequest)）
    qDebug() << "QWebSocket state before open:" << m_webSocket->state();
    m_webSocket->open(request);
    qDebug() << "QWebSocket state after open:" << m_webSocket->state();

    // 检查 WebSocket 是否实际启动
    qDebug() << "WebSocket local address:" << m_webSocket->localAddress().toString();
    qDebug() << "WebSocket local port:" << m_webSocket->localPort();
    qDebug() << "WebSocket peer address:" << m_webSocket->peerAddress().toString();
    qDebug() << "WebSocket peer port:" << m_webSocket->peerPort();
    qDebug() << "WebSocket is valid:" << m_webSocket->isValid();

    // 如果立刻有错误，输出
    if (m_webSocket->error() != QAbstractSocket::UnknownSocketError) {
        qWarning() << "WebSocket immediate error:" << m_webSocket->errorString();
    }
    
    // 添加状态检查定时器
    QTimer* statusTimer = new QTimer(this);
    statusTimer->setInterval(1000); // 每秒检查一次
    QObject::connect(statusTimer, &QTimer::timeout, this, [this, statusTimer]() {
        qDebug() << "=== WebSocket Status Check ===";
        qDebug() << "State:" << m_webSocket->state();
        qDebug() << "Local address:" << m_webSocket->localAddress().toString();
        qDebug() << "Local port:" << m_webSocket->localPort();
        qDebug() << "Peer address:" << m_webSocket->peerAddress().toString();
        qDebug() << "Peer port:" << m_webSocket->peerPort();
        qDebug() << "Is valid:" << m_webSocket->isValid();
        qDebug() << "Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
        
        // 如果连接成功或失败，停止定时器
        if (m_webSocket->state() == QAbstractSocket::ConnectedState || 
            m_webSocket->state() == QAbstractSocket::UnconnectedState) {
            statusTimer->stop();
            statusTimer->deleteLater();
        }
    });
    statusTimer->start();
    
    return true;
}

void AsrQtClient::disconnect() {
    QMutexLocker locker(&m_mutex);
    
    if (m_webSocket) {
        m_webSocket->close();
    }
    
    m_connectionTimer->stop();
    m_connected = false;
}

bool AsrQtClient::isConnected() const {
    return m_connected;
}

bool AsrQtClient::sendAudio(const QByteArray& audioData, bool isLast) {
    if (!m_connected) {
        qWarning() << "ASR client is not connected";
        return false;
    }
    
    // 创建音频消息
    QByteArray message = createBinaryMessage(
        isLast ? AUDIO_ONLY_CLIENT_REQUEST : AUDIO_ONLY_CLIENT_REQUEST,
        isLast ? NEGATIVE_SEQUENCE_SERVER_ASSIGN : NO_SEQUENCE_NUMBER,
        JSON,
        GZIP,
        gzipCompress(audioData)
    );
    
    qint64 bytesSent = m_webSocket->sendBinaryMessage(message);
    if (bytesSent == -1) {
        qWarning() << "Failed to send audio data";
        return false;
    }
    
    qDebug() << "Audio data sent, size:" << audioData.size() << "bytes";
    return true;
}

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

QString AsrQtClient::getFullClientRequestJson() const {
    QJsonObject reqObj;
    
    // app section
    QJsonObject appObj;
    appObj["appid"] = m_appId;
    appObj["cluster"] = m_cluster;
    appObj["token"] = m_token;
    reqObj["app"] = appObj;
    
    // user section
    QJsonObject userObj;
    userObj["uid"] = m_uid;
    reqObj["user"] = userObj;
    
    // request section
    QJsonObject requestObj;
    requestObj["reqid"] = m_reqId;
    requestObj["nbest"] = m_nbest;
    requestObj["workflow"] = m_workflow;
    requestObj["show_language"] = m_showLanguage;
    requestObj["show_utterances"] = m_showUtterances;
    requestObj["result_type"] = m_resultType;
    requestObj["sequence"] = m_seq;
    reqObj["request"] = requestObj;
    
    // audio section
    QJsonObject audioObj;
    audioObj["format"] = m_format;
    audioObj["rate"] = m_sampleRate;
    audioObj["language"] = m_language;
    audioObj["bits"] = m_bits;
    audioObj["channel"] = m_channels;
    audioObj["codec"] = m_codec;
    reqObj["audio"] = audioObj;
    
    QJsonDocument doc(reqObj);
    return doc.toJson(QJsonDocument::Compact);
}

void AsrQtClient::onWebSocketConnected() {
    QMutexLocker locker(&m_mutex);
    
    // 停止连接超时定时器
    m_connectionTimer->stop();
    
    m_connected = true;
    qDebug() << "WebSocket connected to ASR server";
    
    // 发送初始化参数
    if (!sendParams()) {
        qWarning() << "Failed to send initial parameters";
        disconnect();
        return;
    }
    
    // 通知回调
    if (m_callback) {
        m_callback->onOpen(this);
    }
    
    emit connected();
}

void AsrQtClient::onWebSocketDisconnected() {
    QMutexLocker locker(&m_mutex);
    
    // 停止连接超时定时器
    m_connectionTimer->stop();
    
    m_connected = false;
    qDebug() << "WebSocket disconnected from ASR server";
    
    // 通知回调
    if (m_callback) {
        m_callback->onClose(this);
    }
    
    emit disconnected();
}

void AsrQtClient::onWebSocketError(QAbstractSocket::SocketError error) {
    QMutexLocker locker(&m_mutex);
    
    m_connectionTimer->stop();
    
    QString errorMsg = m_webSocket->errorString();
    qWarning() << "WebSocket error:" << error << "-" << errorMsg
               << "URL:" << m_url
               << "Token:" << m_token.left(8) << "...";
    
    if (m_callback) {
        m_callback->onError(this, errorMsg);
    }
    
    emit errorOccurred(errorMsg);
}

void AsrQtClient::onWebSocketTextMessageReceived(const QString& message) {
    qDebug() << "Received text message:" << message;
    
    // 通知回调
    if (m_callback) {
        m_callback->onMessage(this, message);
    }
    
    emit messageReceived(message);
}

void AsrQtClient::onWebSocketBinaryMessageReceived(const QByteArray& message) {
    qDebug() << "Received binary message, size:" << message.size();
    
    QString payloadMsg;
    int ret = parseResponse(message, payloadMsg);
    
    if (ret == 0) {
        // 通知回调
        if (m_callback) {
            m_callback->onMessage(this, payloadMsg);
        }
        
        emit messageReceived(payloadMsg);
    } else {
        qWarning() << "Failed to parse response, ret:" << ret;
    }
}

void AsrQtClient::onConnectionTimeout() {
    QMutexLocker locker(&m_mutex);
    
    qWarning() << "Connection timeout";
    
    // 通知回调
    if (m_callback) {
        m_callback->onError(this, "Connection timeout");
    }
    
    emit errorOccurred("Connection timeout");
}

void AsrQtClient::constructParams() {
    // 构建JSON请求
    QString jsonStr = getFullClientRequestJson();
    QByteArray jsonData = jsonStr.toUtf8();
    
    // 压缩JSON数据
    QByteArray compressedData = gzipCompress(jsonData);
    
    // 创建二进制消息
    m_fullReqParam = createBinaryMessage(
        FULL_CLIENT_REQUEST,
        NO_SEQUENCE_NUMBER,
        JSON,
        GZIP,
        compressedData
    );
    
    qDebug() << "Constructed full request, size:" << m_fullReqParam.size();
    qDebug() << "Request JSON:" << jsonStr;
}

void AsrQtClient::setAuthHeader() {
    // 对于token认证，我们已经在URL中添加了token参数
    // 这里可以添加其他认证方式的支持
}

bool AsrQtClient::sendParams() {
    qDebug() << "Sending initial parameters";
    
    constructParams();
    setAuthHeader();
    
    qint64 bytesSent = m_webSocket->sendBinaryMessage(m_fullReqParam);
    if (bytesSent == -1) {
        qWarning() << "Failed to send binary message";
        return false;
    }
    
    qDebug() << "Initial parameters sent successfully";
    return true;
}

int AsrQtClient::parseResponse(const QByteArray& response, QString& payloadMsg) {
    if (response.size() < 4) {
        qWarning() << "Response too short";
        return -1;
    }
    
    // 解析header
    uint8_t headerSize = (response[0] & 0x0f) << 2;
    uint8_t messageType = (response[1] & 0xf0) >> 4;
    uint8_t messageSerial = (response[2] & 0xf0) >> 4;
    uint8_t messageCompress = response[2] & 0x0f;
    
    qDebug() << "Response header - type:" << messageType 
             << "serial:" << messageSerial 
             << "compress:" << messageCompress;
    
    // 解析payload
    uint32_t payloadLen = 0;
    uint32_t payloadOffset = headerSize;
    
    if (static_cast<MessageType>(messageType) == FULL_SERVER_RESPONSE) {
        if (response.size() < headerSize + 4) {
            qWarning() << "Response too short for FULL_SERVER_RESPONSE";
            return -1;
        }
        
        QDataStream stream(response.mid(headerSize, 4));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> payloadLen;
        payloadOffset = headerSize + 4;
    } else if (static_cast<MessageType>(messageType) == SERVER_ACK) {
        if (response.size() < headerSize + 4) {
            qWarning() << "Response too short for SERVER_ACK";
            return -1;
        }
        
        uint32_t seq;
        QDataStream stream(response.mid(headerSize, 4));
        stream.setByteOrder(QDataStream::BigEndian);
        stream >> seq;
        
        if (response.size() > headerSize + 8) {
            QDataStream lenStream(response.mid(headerSize + 4, 4));
            lenStream.setByteOrder(QDataStream::BigEndian);
            lenStream >> payloadLen;
            payloadOffset = headerSize + 8;
        }
    } else if (static_cast<MessageType>(messageType) == ERROR_MESSAGE_FROM_SERVER) {
        if (response.size() < headerSize + 8) {
            qWarning() << "Response too short for ERROR_MESSAGE_FROM_SERVER";
            return -1;
        }
        
        uint32_t errorCode;
        QDataStream errorStream(response.mid(headerSize, 4));
        errorStream.setByteOrder(QDataStream::BigEndian);
        errorStream >> errorCode;
        
        QDataStream lenStream(response.mid(headerSize + 4, 4));
        lenStream.setByteOrder(QDataStream::BigEndian);
        lenStream >> payloadLen;
        payloadOffset = headerSize + 8;
        
        qWarning() << "Server error code:" << errorCode;
    }
    
    // 处理payload
    if (payloadLen > 0 && response.size() >= payloadOffset + payloadLen) {
        QByteArray payload = response.mid(payloadOffset, payloadLen);
        
        if (static_cast<MessageCompress>(messageCompress) == GZIP) {
            payload = gzipDecompress(payload);
        }
        
        if (static_cast<MessageSerial>(messageSerial) == JSON) {
            payloadMsg = QString::fromUtf8(payload);
        }
    }
    
    return 0;
}

QByteArray AsrQtClient::gzipCompress(const QByteArray& data) {
    if (data.isEmpty()) {
        return QByteArray();
    }
    
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
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
    return compressed;
}

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

QByteArray AsrQtClient::createBinaryMessage(MessageType type, MessageTypeFlag flag, 
                                           MessageSerial serial, MessageCompress compress,
                                           const QByteArray& payload) {
    QByteArray message;
    
    // Header (4 bytes)
    message.append(static_cast<char>(m_headerSize >> 2));
    message.append(static_cast<char>(type << 4 | flag));
    message.append(static_cast<char>(serial << 4 | compress));
    message.append(static_cast<char>(m_reserved));
    
    // Payload length (4 bytes, big endian)
    uint32_t payloadLen = payload.size();
    QDataStream stream(&message, QIODevice::Append);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << payloadLen;
    
    // Payload
    message.append(payload);
    
    return message;
}

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

} 