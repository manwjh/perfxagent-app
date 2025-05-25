#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QWebSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>

class ServerSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ServerSettingsWidget(QWidget* parent = nullptr);
    ~ServerSettingsWidget();

private slots:
    void onTestConnection();
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketTextMessageReceived(const QString& message);

private:
    void initUI();
    void sendHelloMessage();

    QLineEdit* serverUrlEdit_ = nullptr;
    QLineEdit* accessTokenEdit_ = nullptr;
    QLineEdit* deviceIdEdit_ = nullptr;
    QLineEdit* clientIdEdit_ = nullptr;
    QPushButton* testButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QWebSocket* webSocket_ = nullptr;
}; 