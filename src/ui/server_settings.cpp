#include "ui/server_settings.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>

ServerSettingsWidget::ServerSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , serverUrlEdit_(nullptr)
    , accessTokenEdit_(nullptr)
    , deviceIdEdit_(nullptr)
    , clientIdEdit_(nullptr)
    , testButton_(nullptr)
    , statusLabel_(nullptr)
    , webSocket_(nullptr) {
    initUI();
}

ServerSettingsWidget::~ServerSettingsWidget() {
    if (webSocket_) {
        webSocket_->close();
        delete webSocket_;
    }
}

void ServerSettingsWidget::initUI() {
    auto* layout = new QVBoxLayout(this);

    // 服务器设置组
    auto* serverGroup = new QGroupBox("服务器设置", this);
    auto* serverLayout = new QVBoxLayout(serverGroup);

    // 服务器URL
    serverUrlEdit_ = new QLineEdit(this);
    serverUrlEdit_->setPlaceholderText("WebSocket服务器URL (例如: ws://localhost:8080)");
    serverLayout->addWidget(new QLabel("服务器URL:", this));
    serverLayout->addWidget(serverUrlEdit_);

    // Access Token
    accessTokenEdit_ = new QLineEdit(this);
    accessTokenEdit_->setPlaceholderText("访问令牌");
    serverLayout->addWidget(new QLabel("Access Token:", this));
    serverLayout->addWidget(accessTokenEdit_);

    // Device ID
    deviceIdEdit_ = new QLineEdit(this);
    deviceIdEdit_->setPlaceholderText("设备MAC地址");
    serverLayout->addWidget(new QLabel("Device ID:", this));
    serverLayout->addWidget(deviceIdEdit_);

    // Client ID
    clientIdEdit_ = new QLineEdit(this);
    clientIdEdit_->setPlaceholderText("设备UUID");
    serverLayout->addWidget(new QLabel("Client ID:", this));
    serverLayout->addWidget(clientIdEdit_);

    layout->addWidget(serverGroup);

    // 测试按钮
    testButton_ = new QPushButton("测试连接", this);
    testButton_->setStyleSheet(
        "QPushButton {"
        "    background-color: #2196F3;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1976D2;"
        "}"
        "QPushButton:disabled {"
        "    background-color: #9E9E9E;"
        "}"
    );
    connect(testButton_, &QPushButton::clicked, this, &ServerSettingsWidget::onTestConnection);
    layout->addWidget(testButton_);

    // 状态标签
    statusLabel_ = new QLabel("未连接", this);
    statusLabel_->setStyleSheet("color: #757575;");
    layout->addWidget(statusLabel_);

    layout->addStretch();
}

void ServerSettingsWidget::onTestConnection() {
    if (webSocket_) {
        webSocket_->close();
        delete webSocket_;
        webSocket_ = nullptr;
    }

    QString url = serverUrlEdit_->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入服务器URL");
        return;
    }

    webSocket_ = new QWebSocket();
    connect(webSocket_, &QWebSocket::connected, this, &ServerSettingsWidget::onWebSocketConnected);
    connect(webSocket_, &QWebSocket::disconnected, this, &ServerSettingsWidget::onWebSocketDisconnected);
    connect(webSocket_, &QWebSocket::errorOccurred, this, &ServerSettingsWidget::onWebSocketError);
    connect(webSocket_, &QWebSocket::textMessageReceived, this, &ServerSettingsWidget::onWebSocketTextMessageReceived);

    // 设置请求头
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QString("Bearer %1").arg(accessTokenEdit_->text()).toUtf8());
    request.setRawHeader("Protocol-Version", "1");
    request.setRawHeader("Device-Id", deviceIdEdit_->text().toUtf8());
    request.setRawHeader("Client-Id", clientIdEdit_->text().toUtf8());

    webSocket_->open(request);
    testButton_->setEnabled(false);
    statusLabel_->setText("正在连接...");
    statusLabel_->setStyleSheet("color: #FFA000;");
}

void ServerSettingsWidget::onWebSocketConnected() {
    statusLabel_->setText("已连接，正在发送hello消息...");
    statusLabel_->setStyleSheet("color: #4CAF50;");
    sendHelloMessage();
}

void ServerSettingsWidget::onWebSocketDisconnected() {
    testButton_->setEnabled(true);
    statusLabel_->setText("已断开连接");
    statusLabel_->setStyleSheet("color: #757575;");
}

void ServerSettingsWidget::onWebSocketError(QAbstractSocket::SocketError error) {
    testButton_->setEnabled(true);
    statusLabel_->setText(QString("连接错误: %1").arg(webSocket_->errorString()));
    statusLabel_->setStyleSheet("color: #F44336;");
}

void ServerSettingsWidget::onWebSocketTextMessageReceived(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj["type"].toString() == "hello") {
            statusLabel_->setText("连接成功！服务器已响应hello消息");
            statusLabel_->setStyleSheet("color: #4CAF50;");
            testButton_->setEnabled(true);
        }
    }
}

void ServerSettingsWidget::sendHelloMessage() {
    QJsonObject helloMsg;
    helloMsg["type"] = "hello";
    helloMsg["version"] = 1;
    helloMsg["transport"] = "websocket";

    QJsonObject audioParams;
    audioParams["format"] = "opus";
    audioParams["sample_rate"] = 16000;
    audioParams["channels"] = 1;
    audioParams["frame_duration"] = 60;
    helloMsg["audio_params"] = audioParams;

    QJsonDocument doc(helloMsg);
    webSocket_->sendTextMessage(doc.toJson());
} 