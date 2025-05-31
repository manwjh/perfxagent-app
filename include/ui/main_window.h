#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include "../../include/ui/device_settings.h"
#include "ui/agent_zoo.h"
#include "ui/hello_window.h"

namespace perfx {
namespace ui {

class DeviceSettings;
class AgentZoo;
class HelloWindow;

// 主窗口，包含所有Tab页
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onDeviceChanged(const QString& deviceId);
    void onRecordingStarted();
    void onRecordingStopped();

private:
    void initUI();
    void setupConnections();
    void setupUi();
    void createConnections();

    QTabWidget* tabWidget_;
    DeviceSettings* deviceSettings_;
    AgentZoo* agentZoo_;
    HelloWindow* helloWindow_;
    QStatusBar* statusBar_;
};

} // namespace ui
} // namespace perfx 