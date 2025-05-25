#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>
#include <vector>
#include "audio/audio_device.h"

namespace perfx {
namespace ui {

class AudioWaveformWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY(AudioWaveformWidget)
public:
    explicit AudioWaveformWidget(QWidget* parent = nullptr);
    void updateWaveform(const std::vector<float>& data);
    void setSampleRate(int sampleRate, int channels);
    void setPlaybackMode(bool isPlayback);

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    std::vector<float> waveformData_;
    int sampleRate_ = 48000;
    int channels_ = 1;
    float ripplePhase_ = 0.0f;
    bool isPlaybackMode_ = false;
    QTimer* animationTimer_ = nullptr;
    static constexpr int RIPPLE_COUNT = 3;
    static constexpr float RIPPLE_SPEED = 0.02f;
};

class AudioSettingsWidget : public QWidget {
    Q_OBJECT
    Q_DISABLE_COPY(AudioSettingsWidget)
public:
    explicit AudioSettingsWidget(QWidget* parent = nullptr);
    ~AudioSettingsWidget();

signals:
    void deviceChanged(int deviceId);
    void recordingStarted();
    void recordingStopped();

private slots:
    void onDeviceChanged(int index);
    void onSampleRateChanged(const QString& rate);
    void startTestRecording();
    void stopTestRecording();
    void onPlaybackFinished();
    void onAudioData(const float* data, int frames);

private:
    void initUI();
    void initializeAudioDevice();
    void updateDeviceList();
    void updateButtonState(bool isRecording, bool isPlaying);

    QComboBox* deviceCombo_ = nullptr;
    QComboBox* sampleRateCombo_ = nullptr;
    QPushButton* testButton_ = nullptr;
    AudioWaveformWidget* waveformWidget_ = nullptr;
    bool isRecording_ = false;
    bool isPlaying_ = false;
    std::vector<float> recordedAudio_;
    perfx::AudioDevice audioDevice_;
};

} // namespace ui
} // namespace perfx 