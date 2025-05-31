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
#include <QProgressBar>
#include <QPushButton>

namespace perfx {
namespace ui {

AgentZoo::AgentZoo(QWidget* parent)
    : QWidget(parent)
    , webView_(new QWebEngineView(this))
    , statusLabel_(new QLabel(this))
    , webSocket_(nullptr)
    , heartbeatTimer_(new QTimer(this))
    , isConnected_(false)
    , progressBar_(new QProgressBar(this))
    , refreshButton_(new QPushButton("刷新", this))
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
    
    // 添加工具栏
    auto* toolbarLayout = new QHBoxLayout;
    toolbarLayout->setContentsMargins(10, 5, 10, 5);
    
    // 状态标签
    statusLabel_->setStyleSheet(R"(
        QLabel {
            color: #dc3545;
            font-size: 14px;
            padding: 5px;
        }
    )");
    toolbarLayout->addWidget(statusLabel_);
    
    // 进度条
    progressBar_->setStyleSheet(R"(
        QProgressBar {
            border: none;
            background-color: #2d2d2d;
            border-radius: 4px;
            text-align: center;
            height: 4px;
        }
        QProgressBar::chunk {
            background-color: #007bff;
            border-radius: 4px;
        }
    )");
    progressBar_->setFixedHeight(4);
    progressBar_->setTextVisible(false);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    toolbarLayout->addWidget(progressBar_);
    
    // 刷新按钮
    refreshButton_->setStyleSheet(R"(
        QPushButton {
            padding: 5px 10px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 4px;
            min-width: 60px;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    toolbarLayout->addWidget(refreshButton_);
    
    mainLayout->addLayout(toolbarLayout);
    
    // WebView
    webView_->setStyleSheet(R"(
        QWebEngineView {
            background-color: #1e1e1e;
            border: none;
        }
    )");
    mainLayout->addWidget(webView_);
    
    webView_->load(QUrl("https://aiot.llmworld.net/#/login"));
}

void AgentZoo::createConnections() {
    connect(heartbeatTimer_, &QTimer::timeout,
            this, &AgentZoo::onHeartbeatTimeout);
    connect(refreshButton_, &QPushButton::clicked,
            this, &AgentZoo::onRefreshClicked);
    connect(webView_, &QWebEngineView::loadProgress,
            this, &AgentZoo::onLoadProgress);
    connect(webView_, &QWebEngineView::loadFinished,
            this, &AgentZoo::onLoadFinished);
}

void AgentZoo::onLoadProgress(int progress) {
    progressBar_->setValue(progress);
}

void AgentZoo::onLoadFinished(bool ok) {
    if (!ok) {
        statusLabel_->setText("加载失败");
        statusLabel_->setStyleSheet("color: #dc3545;");
    }
    progressBar_->setValue(100);
    QTimer::singleShot(500, [this]() {
        progressBar_->setValue(0);
    });
}

void AgentZoo::onRefreshClicked() {
    webView_->reload();
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