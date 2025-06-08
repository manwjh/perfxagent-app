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
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include "audio/audio_types.h"
#include "audio/audio_device.h"
#include "audio/audio_manager.h"
#include "audio/audio_thread.h"

namespace perfx {
namespace ui {

class DeviceSettings : public QWidget {
    Q_OBJECT
public:
    explicit DeviceSettings(QWidget* parent = nullptr);
    ~DeviceSettings() = default;

signals:
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
    void onTestRecording();
    void onSaveSettings();
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

    // 音频设备
    audio::AudioDevice* audioDevice_;
    bool isPlaying_;
    bool isRecording_;
    std::vector<float> recordedAudio_;
    void updateButtonState(bool isRecording, bool isPlaying);
    void onAudioData(const float* data, int frames);

    // 音频管理器和设备列表
    audio::AudioManager& audioManager_;
    std::vector<audio::DeviceInfo> devices_;
    audio::AudioConfig audioConfig_;
    std::shared_ptr<audio::AudioThread> audioThread_;
};

} // namespace ui
} // namespace perfx 