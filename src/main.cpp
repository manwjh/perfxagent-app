#include <QApplication>
#include <QMainWindow>
#include "ui/audio_settings.h"
#include <iostream>

class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("PerfXAgent客户端应用程序");
        resize(800, 600);

        // 创建音频设置界面
        audioSettings_ = new AudioSettingsWidget(this);
        setCentralWidget(audioSettings_);

        // 连接信号
        connect(audioSettings_, &AudioSettingsWidget::deviceChanged,
                this, &MainWindow::onDeviceChanged);
        connect(audioSettings_, &AudioSettingsWidget::recordingStarted,
                this, &MainWindow::onRecordingStarted);
        connect(audioSettings_, &AudioSettingsWidget::recordingStopped,
                this, &MainWindow::onRecordingStopped);
    }

private slots:
    void onDeviceChanged(int deviceId) {
        std::cout << "设备已更改: " << deviceId << std::endl;
    }

    void onRecordingStarted() {
        std::cout << "开始录音" << std::endl;
    }

    void onRecordingStopped() {
        std::cout << "停止录音" << std::endl;
    }

private:
    AudioSettingsWidget* audioSettings_;
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
} 