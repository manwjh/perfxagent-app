#include "ui/main_window.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>

namespace perfx {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , tabWidget_(new QTabWidget(this))
    , deviceSettings_(new DeviceSettings(this))
    , agentZoo_(new AgentZoo(this))
    , helloWindow_(new HelloWindow(this))
    , statusBar_(new QStatusBar(this))
{
    setupUi();
    createConnections();
}

void MainWindow::setupUi() {
    setWindowTitle("Perfx Agent 应用");
    resize(1024, 768);
    setCentralWidget(tabWidget_);

    setStatusBar(statusBar_);
    statusBar_->showMessage("就绪");

    tabWidget_->addTab(helloWindow_, "Hello AI");
    tabWidget_->addTab(agentZoo_, "Agent Zoo");
    tabWidget_->addTab(deviceSettings_, "系统设置");

    tabWidget_->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #ddd;
            border-radius: 6px;
            background: #f0f0f0;
            padding: 10px;
        }
        QTabBar::tab {
            min-width: 120px;
            min-height: 32px;
            font-size: 15px;
            padding: 8px 20px;
            border-radius: 6px 6px 0 0;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: #e0e0e0;
            color: #007bff;
            font-weight: bold;
            border-bottom: 2px solid #007bff;
        }
        QTabBar::tab:!selected {
            background: #f0f0f0;
            color: #333;
        }
        QTabBar::tab:hover {
            background: #e8e8e8;
        }
    )");
}

void MainWindow::createConnections() {
    connect(deviceSettings_, &DeviceSettings::deviceChanged,
            this, &MainWindow::onDeviceChanged);
    connect(deviceSettings_, &DeviceSettings::recordingStarted,
            this, &MainWindow::onRecordingStarted);
    connect(deviceSettings_, &DeviceSettings::recordingStopped,
            this, &MainWindow::onRecordingStopped);
}

void MainWindow::onDeviceChanged(const QString& deviceId) { (void)deviceId; }
void MainWindow::onRecordingStarted() {}
void MainWindow::onRecordingStopped() {}

MainWindow::~MainWindow() {}

} // namespace ui
} // namespace perfx 