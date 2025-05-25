#pragma once

#include <QMainWindow>
#include "ui/audio_settings.h"
#include "ui/server_settings.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onAudioDeviceChanged(int deviceId);
    void onRecordingStarted();
    void onRecordingStopped();

private:
    perfx::ui::AudioSettingsWidget* audioSettings_;
    ServerSettingsWidget* serverSettings_;
}; 