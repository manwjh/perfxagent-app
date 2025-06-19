#include <QCoreApplication>
#include <QWebSocket>
#include <QTimer>
#include <QDebug>
#include <QSslConfiguration>
#include <QNetworkProxy>
#include <iostream>
#include "asr/asr_qt_client.h"

#define DEFAULT_ASR_WS_URL "wss://openspeech.bytedance.com/api/v2/asr"

class WebSocketTest : public QObject
{
    Q_OBJECT

public:
    WebSocketTest(QObject *parent = nullptr) : QObject(parent)
    {
        m_webSocket = new QWebSocket();
        m_webSocket->setParent(this);
        
        // 设置 SSL 配置
        QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
        sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
        sslConfig.setProtocol(QSsl::TlsV1_2OrLater);
        m_webSocket->setSslConfiguration(sslConfig);
        
        // 设置代理
        m_webSocket->setProxy(QNetworkProxy::NoProxy);
        
        // 连接信号
        connect(m_webSocket, &QWebSocket::connected, this, &WebSocketTest::onConnected);
        connect(m_webSocket, &QWebSocket::disconnected, this, &WebSocketTest::onDisconnected);
        connect(m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
                this, &WebSocketTest::onError);
        connect(m_webSocket, &QWebSocket::stateChanged, this, &WebSocketTest::onStateChanged);
        connect(m_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketTest::onTextMessageReceived);
        
        // 设置超时定时器
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, &WebSocketTest::onTimeout);
        
        // 设置状态检查定时器
        m_statusTimer = new QTimer(this);
        m_statusTimer->setInterval(1000);
        connect(m_statusTimer, &QTimer::timeout, this, &WebSocketTest::onStatusCheck);
    }
    
    void testConnection(const QString &url)
    {
        qDebug() << "Testing WebSocket connection to:" << url;
        
        // 获取安全凭据
        Asr::AsrQtClient::Credentials creds = Asr::AsrQtClient::getCredentialsFromEnv();
        if (!creds.isValid) {
            qWarning() << "❌ 凭据无效，无法继续";
            return;
        }
        
        QUrl wsUrl(url);
        QNetworkRequest request(wsUrl);
        
        // 设置一些基本头
        request.setRawHeader("User-Agent", "Qt6-WebSocket-Test/1.0");
        request.setRawHeader("Host", "openspeech.bytedance.com");
        request.setRawHeader("Accept", "*/*");
        
        // 使用环境变量中的token
        QString token = creds.accessToken;
        if (!token.isEmpty()) {
            request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
            qDebug() << "[WS] Authorization header set with token from environment variables";
        } else {
            qDebug() << "[WS] Warning: No token available";
        }
        
        qDebug() << "Opening WebSocket...";
        m_webSocket->open(request);
        
        // 启动超时定时器（10秒）
        m_timeoutTimer->start(10000);
        m_statusTimer->start();
    }

private slots:
    void onConnected()
    {
        qDebug() << "WebSocket connected successfully!";
        qDebug() << "=== 连接详细信息 ===";
        qDebug() << "Local address:" << m_webSocket->localAddress().toString();
        qDebug() << "Local port:" << m_webSocket->localPort();
        qDebug() << "Peer address:" << m_webSocket->peerAddress().toString();
        qDebug() << "Peer port:" << m_webSocket->peerPort();
        qDebug() << "Is valid:" << m_webSocket->isValid();
        qDebug() << "State:" << m_webSocket->state();
        qDebug() << "Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
        qDebug() << "=====================";
        
        m_timeoutTimer->stop();
        m_statusTimer->stop();
        
        // 测试连接稳定性，不发送任何消息
        qDebug() << "✅ 连接测试成功 - 仅测试WebSocket连接稳定性";
        qDebug() << "注意: 未发送ASR协议消息，仅验证网络连接";
        
        // 等待一段时间观察连接状态
        QTimer::singleShot(2000, this, [this]() {
            if (m_webSocket->state() == QAbstractSocket::ConnectedState) {
                qDebug() << "✅ 连接保持稳定，测试通过";
            } else {
                qWarning() << "❌ 连接不稳定，状态:" << m_webSocket->state();
            }
            qDebug() << "Closing test connection...";
            m_webSocket->close();
        });
    }
    
    void onDisconnected()
    {
        qDebug() << "WebSocket disconnected";
        m_timeoutTimer->stop();
        m_statusTimer->stop();
        QCoreApplication::quit();
    }
    
    void onError(QAbstractSocket::SocketError error)
    {
        qWarning() << "WebSocket error:" << error << "-" << m_webSocket->errorString();
        m_timeoutTimer->stop();
        m_statusTimer->stop();
        QCoreApplication::quit();
    }
    
    void onStateChanged(QAbstractSocket::SocketState state)
    {
        qDebug() << "WebSocket state changed to:" << state;
    }
    
    void onTimeout()
    {
        qWarning() << "WebSocket connection timeout";
        m_statusTimer->stop();
        m_webSocket->close();
        QCoreApplication::quit();
    }
    
    void onStatusCheck()
    {
        qDebug() << "=== Status Check ===";
        qDebug() << "State:" << m_webSocket->state();
        qDebug() << "Local address:" << m_webSocket->localAddress().toString();
        qDebug() << "Local port:" << m_webSocket->localPort();
        qDebug() << "Peer address:" << m_webSocket->peerAddress().toString();
        qDebug() << "Peer port:" << m_webSocket->peerPort();
        qDebug() << "Is valid:" << m_webSocket->isValid();
        qDebug() << "Error:" << m_webSocket->error() << "-" << m_webSocket->errorString();
    }

    void onTextMessageReceived(const QString &message)
    {
        qDebug() << "Received message from server:" << message;
    }

private:
    QWebSocket *m_webSocket;
    QTimer *m_timeoutTimer;
    QTimer *m_statusTimer;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // 显示环境变量版本信息
    std::cout << "=== WebSocket测试程序 - 环境变量版本 ===" << std::endl;
    std::cout << "优先从环境变量读取凭据，未设置时使用默认值" << std::endl;
    std::cout << "" << std::endl;
    
    QString url = DEFAULT_ASR_WS_URL;
    if (argc >= 2) {
        url = argv[1];
    }
    qDebug() << "Using WebSocket URL:" << url;
    
    WebSocketTest test;
    test.testConnection(url);
    
    return app.exec();
}

#include "test_qt_websocket.moc" 