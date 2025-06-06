#pragma once

#include <QWidget>
#include <QWebEngineView>
#include <QLabel>
#include <QWebSocket>
#include <QTimer>
#include <QProgressBar>
#include <QPushButton>

namespace perfx {
namespace ui {

// Agent绑定界面和功能模块
class AgentZoo : public QWidget {
    Q_OBJECT

public:
    explicit AgentZoo(QWidget* parent = nullptr);
    ~AgentZoo() = default;

    // 设置WebSocket连接参数
    void setWebSocketParams(const QString& url, const QString& accessToken,
                           const QString& deviceId, const QString& clientId);

signals:
    void connectionStateChanged(bool connected);

private slots:
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketMessage(const QString& message);
    void onHeartbeatTimeout();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);
    void onRefreshClicked();

private:
    void setupUi();
    void createConnections();
    void updateStatus(bool connected);
    void sendHeartbeat();

    QWebEngineView* webView_;
    QLabel* statusLabel_;
    QWebSocket* webSocket_;
    QTimer* heartbeatTimer_;
    bool isConnected_;
    QString serverUrl_;
    QString accessToken_;
    QString deviceId_;
    QString clientId_;
    QProgressBar* progressBar_;
    QPushButton* refreshButton_;
};

} // namespace ui
} // namespace perfx 