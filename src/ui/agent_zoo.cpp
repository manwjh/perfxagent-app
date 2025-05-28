#include "ui/agent_zoo.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebSocket>
#include <QTimer>
#include <QLabel>

namespace perfx {
namespace ui {

AgentZoo::AgentZoo(QWidget* parent)
    : QWidget(parent)
    , webView_(new QWebEngineView(this))
    , statusLabel_(new QLabel(this))
    , webSocket_(nullptr)
    , heartbeatTimer_(new QTimer(this))
    , isConnected_(false)
{
    setupUi();
    createConnections();
}

void AgentZoo::setWebSocketParams(const QString& url, const QString& accessToken,
                                const QString& deviceId, const QString& clientId) {
    serverUrl_ = url;
    accessToken_ = accessToken;
    deviceId_ = deviceId;
    clientId_ = clientId;
    
    if (webSocket_) {
        webSocket_->close();
        webSocket_->deleteLater();
        webSocket_ = nullptr;
    }
    
    // 创建新的 WebSocket 连接
    webSocket_ = new QWebSocket();
    connect(webSocket_, &QWebSocket::connected,
            this, [this]() {
                updateStatus(true);
                heartbeatTimer_->start(30000); // 30秒发送一次心跳
            });
    connect(webSocket_, &QWebSocket::disconnected,
            this, [this]() {
                updateStatus(false);
                heartbeatTimer_->stop();
            });
    connect(webSocket_, &QWebSocket::textMessageReceived,
            this, &AgentZoo::onWebSocketMessage);
    connect(webSocket_, &QWebSocket::errorOccurred,
            this, &AgentZoo::onWebSocketError);
    
    // 构建 WebSocket URL
    QUrl wsUrl(serverUrl_);
    wsUrl.setScheme("ws");
    QUrlQuery query;
    query.addQueryItem("access_token", accessToken_);
    query.addQueryItem("device_id", deviceId_);
    query.addQueryItem("client_id", clientId_);
    wsUrl.setQuery(query);
    
    // 连接服务器
    webSocket_->open(wsUrl);
    
    isConnected_ = false;
    updateStatus(false);
}

void AgentZoo::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(webView_);
    webView_->load(QUrl("https://aiot.llmworld.net/#/login"));
}

void AgentZoo::createConnections() {
    connect(heartbeatTimer_, &QTimer::timeout,
            this, &AgentZoo::onHeartbeatTimeout);
}

void AgentZoo::onWebSocketError(QAbstractSocket::SocketError error) {
    (void)error;
    QString errorMsg = "WebSocket错误: " + webSocket_->errorString();
    statusLabel_->setText(errorMsg);
    statusLabel_->setStyleSheet("color: #dc3545;");
    QMessageBox::warning(this, "错误", errorMsg);
}

void AgentZoo::onWebSocketMessage(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;
    
    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();
    
    if (type == "connection_state") {
        bool connected = obj["connected"].toBool();
        updateStatus(connected);
    }
}

void AgentZoo::onHeartbeatTimeout() {
    if (isConnected_ && webSocket_) {
        sendHeartbeat();
    }
}

void AgentZoo::updateStatus(bool connected) {
    isConnected_ = connected;
    statusLabel_->setText(connected ? "已连接" : "未连接");
    statusLabel_->setStyleSheet(connected ? "color: #28a745;" : "color: #dc3545;");
    emit connectionStateChanged(connected);
}

void AgentZoo::sendHeartbeat() {
    if (!webSocket_) return;
    
    QJsonObject heartbeat;
    heartbeat["type"] = "heartbeat";
    heartbeat["device_id"] = deviceId_;
    heartbeat["client_id"] = clientId_;
    
    webSocket_->sendTextMessage(QJsonDocument(heartbeat).toJson());
}

} // namespace ui
} // namespace perfx 