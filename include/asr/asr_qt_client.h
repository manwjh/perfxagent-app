//
// 纯Qt版本的ASR客户端
// 完全兼容火山ASR WebSocket协议，无外部依赖
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
#include <memory>

namespace Asr {

class AsrQtClient;

class AsrQtCallback {
public:
    virtual ~AsrQtCallback() = default;
    
    // WebSocket连接建立
    virtual void onOpen(AsrQtClient* client) = 0;
    
    // 收到服务器消息
    virtual void onMessage(AsrQtClient* client, const QString& message) = 0;
    
    // 发生错误
    virtual void onError(AsrQtClient* client, const QString& error) = 0;
    
    // 连接关闭
    virtual void onClose(AsrQtClient* client) = 0;
};

class AsrQtClient : public QObject {
    Q_OBJECT

public:
    enum AuthType {
        TOKEN = 1,
        SIGNATURE = 2
    };

    enum AudioType {
        LOCAL = 1,
        URL = 2
    };

    enum MessageType : uint8_t {
        FULL_CLIENT_REQUEST = 0b0001,
        AUDIO_ONLY_CLIENT_REQUEST = 0b0010,
        FULL_SERVER_RESPONSE = 0b1001,
        SERVER_ACK = 0b1011,
        ERROR_MESSAGE_FROM_SERVER = 0b1111
    };

    enum MessageTypeFlag : uint8_t {
        NO_SEQUENCE_NUMBER = 0b0000,
        POSITIVE_SEQUENCE_CLIENT_ASSIGN = 0b0001,
        NEGATIVE_SEQUENCE_SERVER_ASSIGN = 0b0010,
        NEGATIVE_SEQUENCE_CLIENT_ASSIGN = 0b0011
    };

    enum MessageSerial : uint8_t {
        NO_SERIAL = 0b0000,
        JSON = 0b0001,
        CUSTOM_SERIAL = 0b1111
    };

    enum MessageCompress : uint8_t {
        NO_COMPRESS = 0b0000,
        GZIP = 0b0001,
        CUSTOM_COMPRESS = 0b1111
    };

    struct Credentials {
        QString appId;
        QString accessToken;
        QString secretKey;
        bool isValid = false;
    };

    explicit AsrQtClient(QObject *parent = nullptr);
    ~AsrQtClient();

    // 配置方法
    void setAppId(const QString& appId);
    void setToken(const QString& token);
    void setSecretKey(const QString& secretKey);
    void setAuthType(AuthType authType);
    void setAudioFormat(const QString& format, int channels, int sampleRate, int bits);
    void setCluster(const QString& cluster);
    void setCallback(AsrQtCallback* callback);

    // 连接控制
    bool connect();
    void disconnect();
    bool isConnected() const;

    // 发送音频数据
    bool sendAudio(const QByteArray& audioData, bool isLast = false);
    bool sendAudioFile(const QString& filePath);

    // 获取状态信息
    QString getFullClientRequestJson() const;

    // 静态方法：从环境变量获取凭据
    static Credentials getCredentialsFromEnv();

signals:
    void connected();
    void disconnected();
    void messageReceived(const QString& message);
    void errorOccurred(const QString& error);

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextMessageReceived(const QString& message);
    void onWebSocketBinaryMessageReceived(const QByteArray& message);
    void onConnectionTimeout();

private:
    // 协议相关
    void constructParams();
    void setAuthHeader();
    bool sendParams();
    int parseResponse(const QByteArray& response, QString& payloadMsg);
    
    // 工具方法
    QByteArray gzipCompress(const QByteArray& data);
    QByteArray gzipDecompress(const QByteArray& data);
    QByteArray createBinaryMessage(MessageType type, MessageTypeFlag flag, 
                                  MessageSerial serial, MessageCompress compress,
                                  const QByteArray& payload);

private:
    // 配置参数
    QString m_url{"wss://openspeech.bytedance.com/api/v2/asr"};
    QString m_appId;
    QString m_token;
    QString m_secretKey;
    AuthType m_authType{TOKEN};
    QString m_cluster;
    
    // 音频参数
    QString m_format{"wav"};
    int m_sampleRate{16000};
    int m_bits{16};
    int m_channels{1};
    QString m_codec{"raw"};
    
    // 请求参数
    QString m_reqId;
    int32_t m_seq{1};
    QString m_uid{"asr_qt_demo"};
    QString m_workflow{"audio_in,resample,partition,vad,fe,decode"};
    int m_nbest{1};
    bool m_showLanguage{false};
    bool m_showUtterances{false};
    QString m_resultType{"full"};
    QString m_language{"zh-CN"};
    
    // 协议参数
    uint8_t m_protocolVersion{0b0001};
    uint8_t m_headerSize{4};
    uint8_t m_reserved{0};
    
    // 状态管理
    bool m_connected{false};
    bool m_recvLastMsg{false};
    QMutex m_mutex;
    
    // WebSocket相关
    QWebSocket* m_webSocket;
    QTimer* m_connectionTimer;
    
    // 回调
    AsrQtCallback* m_callback{nullptr};
    
    // 缓存
    QByteArray m_fullReqParam;
};

}

#endif // ASR_QT_CLIENT_H 