#include "ui/main_window.h"
#include "ui/audio_settings.h"
#include "ui/server_settings.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <iostream>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setWindowTitle("PerfxAgent");
    resize(800, 600);

    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* layout = new QVBoxLayout(centralWidget);

    // 创建标签页
    auto* tabWidget = new QTabWidget(this);
    
    // 添加音频设置标签页
    audioSettings_ = new perfx::ui::AudioSettingsWidget(this);
    tabWidget->addTab(audioSettings_, "音频设置");

    // 添加服务器设置标签页
    serverSettings_ = new ServerSettingsWidget(this);
    tabWidget->addTab(serverSettings_, "服务器设置");

    layout->addWidget(tabWidget);

    // 连接信号
    connect(audioSettings_, &perfx::ui::AudioSettingsWidget::deviceChanged,
            this, &MainWindow::onAudioDeviceChanged);
    connect(audioSettings_, &perfx::ui::AudioSettingsWidget::recordingStarted,
            this, &MainWindow::onRecordingStarted);
    connect(audioSettings_, &perfx::ui::AudioSettingsWidget::recordingStopped,
            this, &MainWindow::onRecordingStopped);
}

void MainWindow::onAudioDeviceChanged(int deviceId) {
    std::cout << "设备已更改: " << deviceId << std::endl;
}

void MainWindow::onRecordingStarted() {
    std::cout << "开始录音" << std::endl;
}

void MainWindow::onRecordingStopped() {
    std::cout << "停止录音" << std::endl;
} 