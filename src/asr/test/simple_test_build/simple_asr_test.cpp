#include <QCoreApplication>
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <QWebSocket>
#include <QNetworkRequest>
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QSslCipher>
#include <QMutex>
#include <QJsonObject>
#include <QJsonDocument>
#include <QByteArray>
#include <QUuid>
#include <QDataStream>
#include <QIODevice>
#include <QTimer>
#include <QFile>
#include <QJsonValue>
#include <QAbstractSocket>
#include <zlib.h>
#include <QNetworkAccessManager>
#include <QEventLoop>

// 直接包含ASR客户端的头文件
#include "asr_qt_client.h"

class SimpleAsrCallback : public Asr::AsrQtCallback {
public:
    void onOpen(Asr::AsrQtClient* client) override {
        qDebug() << "✅ ASR WebSocket connected successfully!";
        QCoreApplication::quit();
    }
    
    void onMessage(Asr::AsrQtClient* client, const QString& message) override {
        qDebug() << "📨 Received message:" << message;
    }
    
    void onError(Asr::AsrQtClient* client, const QString& error) override {
        qWarning() << "❌ ASR Error:" << error;
        QCoreApplication::quit();
    }
    
    void onClose(Asr::AsrQtClient* client) override {
        qDebug() << "🔌 ASR WebSocket closed";
        QCoreApplication::quit();
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "🔧 Simple ASR Test - Testing basic WebSocket connection";
    
    // 创建ASR客户端
    Asr::AsrQtClient asrClient;
    SimpleAsrCallback callback;
    asrClient.setCallback(&callback);
    
    // 设置基本配置
    asrClient.setAppId("8388344882");
    asrClient.setToken("vQWuOVrg*************ZfA5q2lG3");
    asrClient.setAuthType(Asr::AsrQtClient::TOKEN);
    asrClient.setAudioFormat("wav", 1, 16000, 16);
    
    // 设置超时定时器
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    timeoutTimer.setInterval(10000); // 10秒超时
    QObject::connect(&timeoutTimer, &QTimer::timeout, [&]() {
        qWarning() << "⏰ Connection timeout";
        QCoreApplication::quit();
    });
    
    // 尝试连接
    qDebug() << "🔗 Attempting to connect...";
    if (asrClient.connect()) {
        timeoutTimer.start();
        qDebug() << "✅ Connect() returned true, waiting for connection...";
    } else {
        qWarning() << "❌ Connect() returned false";
        return 1;
    }
    
    return app.exec();
} 