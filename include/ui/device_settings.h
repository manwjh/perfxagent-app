#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>
#include <memory>
#include <QByteArray>
#include <vector>
#include "audio/audio_device.h"
#include "core/config_manager.h"

namespace perfx {
namespace ui {

class DeviceSettings : public QWidget {
    Q_OBJECT
public:
    explicit DeviceSettings(QWidget* parent = nullptr);
    ~DeviceSettings() = default;

signals:
    void settingsChanged(const QString& url, const QString& accessToken, const QString& deviceId, const QString& clientId);
    void deviceChanged(const QString& deviceId);
    void recordingStarted();
    void recordingStopped();

private slots:
    void onDeviceChanged(int index);
    void onSampleRateChanged(int index);
    void onChannelsChanged(int index);
    void onBitDepthChanged(int index);
    void onTestButtonClicked();
    void onSaveButtonClicked();
    void onTestServerButtonClicked();
    void onTestRecording();
    void onSaveSettings();
    void onBrowseRecordingPath();
    void onTestConnection();
    void playbackFinished();

private:
    void setupUi();
    void createConnections();
    void loadSettings();
    void initializeAudioDevice();
    void updateDeviceList();
    void updateStatus(const QString& status, bool isError = false);
    void initUI();
    void setupConnections();
    void onTestButtonPressed();
    void onTestButtonReleased();

    // 音频设备设置
    QComboBox* deviceCombo_;
    QComboBox* sampleRateCombo_;
    QComboBox* channelsCombo_;
    QComboBox* bitDepthCombo_;
    QPushButton* testButton_;
    QPushButton* saveButton_;
    QLabel* waveformLabel_;

    // 服务器设置
    QPushButton* testServerButton_;
    QLabel* statusLabel_;

    // 新增：服务器和录音相关输入框
    QLineEdit* serverUrlEdit_;
    QLineEdit* accessTokenEdit_;
    QLineEdit* deviceIdEdit_;
    QLineEdit* clientIdEdit_;
    QLineEdit* recordingPathEdit_;

    // 音频设备
    perfx::AudioDevice* audioDevice_;
    bool isPlaying_;
    bool isRecording_;
    std::vector<float> recordedAudio_;
    void updateButtonState(bool isRecording, bool isPlaying);
    void updateButtonStateServer(bool isConnected);
    void onAudioData(const float* data, int frames);
};

} // namespace ui
} // namespace perfx 