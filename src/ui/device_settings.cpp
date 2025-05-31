#include "../../include/ui/device_settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QGroupBox>
#include <QDebug>
#include <vector>
#include <thread>
#include <chrono>

using namespace perfx::audio;

namespace perfx {
namespace ui {

DeviceSettings::DeviceSettings(QWidget* parent)
    : QWidget(parent)
    , deviceCombo_(new QComboBox(this))
    , sampleRateCombo_(new QComboBox(this))
    , channelsCombo_(new QComboBox(this))
    , bitDepthCombo_(new QComboBox(this))
    , testButton_(new QPushButton("测试录音", this))
    , saveButton_(new QPushButton("保存设置", this))
    , waveformLabel_(new QLabel(this))
    , audioManager_(AudioManager::getInstance())
{
    initUI();
    loadSettings();
    setupConnections();
    initializeAudioDevice();
}

void DeviceSettings::initUI() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 音频设置组
    auto* audioGroup = new QGroupBox("音频设置", this);
    auto* audioForm = new QFormLayout(audioGroup);
    audioForm->addRow("音频设备:", deviceCombo_);
    sampleRateCombo_->addItems({"16000", "44100", "48000"});
    audioForm->addRow("采样率:", sampleRateCombo_);
    channelsCombo_->addItems({"1", "2"});
    audioForm->addRow("通道数:", channelsCombo_);
    bitDepthCombo_->addItems({"16", "24", "32"});
    audioForm->addRow("位深度:", bitDepthCombo_);
    audioForm->addRow(testButton_);
    audioForm->addRow(waveformLabel_);

    mainLayout->addWidget(audioGroup);
    mainLayout->addWidget(saveButton_);
}

void DeviceSettings::loadSettings() {
    // TODO: 加载服务器和音频设置
}

void DeviceSettings::setupConnections() {
    connect(testButton_, &QPushButton::pressed, this, &DeviceSettings::onTestButtonPressed);
    connect(testButton_, &QPushButton::released, this, &DeviceSettings::onTestButtonReleased);
    connect(saveButton_, &QPushButton::clicked, this, &DeviceSettings::onSaveButtonClicked);
}

void DeviceSettings::initializeAudioDevice() {
    auto& manager = AudioManager::getInstance();
    if (!manager.initialize()) {
        QMessageBox::warning(this, "错误", "音频管理器初始化失败");
        return;
    }
    devices_ = manager.getAvailableDevices();
    deviceCombo_->clear();
    for (const auto& device : devices_) {
        if (device.type == DeviceType::INPUT || device.type == DeviceType::BOTH) {
            deviceCombo_->addItem(QString::fromStdString(device.name));
        }
    }
}

void DeviceSettings::onTestButtonPressed() {
    if (isPlaying_) return;
    recordedAudio_.clear();
    isRecording_ = true;
    updateButtonState(isRecording_, isPlaying_);

    int deviceIdx = deviceCombo_->currentIndex();
    if (deviceIdx < 0 || deviceIdx >= static_cast<int>(devices_.size())) {
        QMessageBox::warning(this, "错误", "请选择音频设备");
        isRecording_ = false;
        updateButtonState(isRecording_, isPlaying_);
        return;
    }
    auto& selectedDevice = devices_[deviceIdx];
    audioConfig_.sampleRate = static_cast<SampleRate>(sampleRateCombo_->currentText().toInt());
    audioConfig_.channels = channelsCombo_->currentText() == "1" ? ChannelCount::MONO : ChannelCount::STEREO;
    audioConfig_.format = SampleFormat::FLOAT32;
    audioConfig_.framesPerBuffer = 256;

    auto& manager = AudioManager::getInstance();
    audioThread_ = manager.createAudioThread(audioConfig_);
    if (!audioThread_) {
        QMessageBox::warning(this, "错误", "无法创建音频线程");
        isRecording_ = false;
        updateButtonState(isRecording_, isPlaying_);
        return;
    }
    if (!audioThread_->setInputDevice(selectedDevice)) {
        QMessageBox::warning(this, "错误", "设置输入设备失败");
        isRecording_ = false;
        updateButtonState(isRecording_, isPlaying_);
        return;
    }
    audioThread_->addProcessor(manager.getProcessor());
    audioThread_->setInputCallback([this](const void* input, size_t frames) {
        const float* data = static_cast<const float*>(input);
        size_t samples = frames * static_cast<int>(audioConfig_.channels);
        recordedAudio_.insert(recordedAudio_.end(), data, data + samples);
    });
    audioThread_->start();
}

void DeviceSettings::onTestButtonReleased() {
    if (!isRecording_) return;
    isRecording_ = false;
    if (audioThread_) audioThread_->stop();
    updateButtonState(isRecording_, isPlaying_);
    if (!recordedAudio_.empty()) {
        isPlaying_ = true;
        updateButtonState(isRecording_, isPlaying_);
        // 播放录音
        int deviceIdx = deviceCombo_->currentIndex();
        if (deviceIdx < 0 || deviceIdx >= static_cast<int>(devices_.size())) return;
        auto& selectedDevice = devices_[deviceIdx];
        auto& manager = AudioManager::getInstance();
        auto playConfig = audioConfig_;
        auto playThread = manager.createAudioThread(playConfig);
        if (!playThread) return;
        if (!playThread->setOutputDevice(selectedDevice)) return;
        playThread->addProcessor(manager.getProcessor());
        playThread->start();
        // 简单模拟播放时长
        std::thread([this, playThread]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            playThread->stop();
            isPlaying_ = false;
            QMetaObject::invokeMethod(this, [this](){ updateButtonState(isRecording_, isPlaying_); }, Qt::QueuedConnection);
        }).detach();
    }
}

void DeviceSettings::onSaveButtonClicked() {
    // TODO: 保存音频设置
}

void DeviceSettings::updateButtonState(bool isRecording, bool isPlaying) {
    testButton_->setEnabled(!isPlaying);
    if (isRecording) {
        testButton_->setText("录音中...");
    } else if (isPlaying) {
        testButton_->setText("播放中...");
    } else {
        testButton_->setText("测试录音");
    }
}

void DeviceSettings::onDeviceChanged(int) {}
void DeviceSettings::onSampleRateChanged(int) {}
void DeviceSettings::onChannelsChanged(int) {}
void DeviceSettings::onBitDepthChanged(int) {}
void DeviceSettings::onTestButtonClicked() {}
void DeviceSettings::onSaveSettings() {}
void DeviceSettings::onTestRecording() {}
void DeviceSettings::playbackFinished() {}

} // namespace ui
} // namespace perfx