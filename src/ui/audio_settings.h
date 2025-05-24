#pragma once

#include <QWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include "audio/audio_device.h"
#include "audio_settings.h"

class AudioWaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit AudioWaveformWidget(QWidget* parent = nullptr);
    void updateWaveform(const std::vector<float>& data);
    void setSampleRate(int sampleRate, int channels);
    void setPlaybackMode(bool isPlayback) { isPlayback_ = isPlayback; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;

private:
    std::vector<float> waveformData_;
    int sampleRate_ = 16000;  // 默认采样率
    int channels_ = 1;        // 默认通道数
    bool isPlayback_ = false;
    QTimer* animationTimer_;
    float ripplePhase_ = 0.0f;
    static constexpr float RIPPLE_SPEED = 0.1f;
    static constexpr int RIPPLE_COUNT = 3;
};

class AudioSettingsWidget : public QWidget {
    Q_OBJECT
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
    void updateDeviceList();
    void updateButtonState(bool isRecording, bool isPlaying);
    void initializeAudioDevice();

    QComboBox* deviceCombo_;
    QComboBox* sampleRateCombo_;
    QPushButton* testButton_;
    AudioWaveformWidget* waveformWidget_;
    AudioDevice audioDevice_;
    std::vector<float> recordedAudio_;
    bool isRecording_;
    bool isPlaying_;
}; 