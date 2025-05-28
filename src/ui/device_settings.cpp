#include "ui/device_settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include "audio/audio_device.h"
#include "core/config_manager.h"
#include <QDebug>
#include <vector>

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
    , testServerButton_(new QPushButton("测试连接", this))
    , statusLabel_(new QLabel(this))
    , serverUrlEdit_(new QLineEdit(this))
    , accessTokenEdit_(new QLineEdit(this))
    , deviceIdEdit_(new QLineEdit(this))
    , clientIdEdit_(new QLineEdit(this))
    , recordingPathEdit_(new QLineEdit(this))
    , audioDevice_(new AudioDevice())
    , isPlaying_(false)
    , isRecording_(false)
    , recordedAudio_()
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

    // 创建左右栏布局
    auto* horizontalLayout = new QHBoxLayout;
    horizontalLayout->setSpacing(10);

    // 服务器设置组
    auto* serverGroup = new QGroupBox("服务器设置", this);
    serverGroup->setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            font-size: 16px;
            border: 1px solid #007bff;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: #1e1e1e;
            color: white;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #007bff;
        }
    )");
    auto* serverForm = new QFormLayout(serverGroup);
    serverForm->setSpacing(8);
    serverUrlEdit_->setPlaceholderText("wss://api.perfxagent.com/ws");
    serverUrlEdit_->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QLineEdit:focus {
            border: 1px solid #00a0ff;
        }
    )");
    accessTokenEdit_->setPlaceholderText("输入访问令牌");
    accessTokenEdit_->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QLineEdit:focus {
            border: 1px solid #00a0ff;
        }
    )");
    deviceIdEdit_->setPlaceholderText("输入设备ID");
    deviceIdEdit_->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QLineEdit:focus {
            border: 1px solid #00a0ff;
        }
    )");
    clientIdEdit_->setPlaceholderText("输入客户端ID");
    clientIdEdit_->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QLineEdit:focus {
            border: 1px solid #00a0ff;
        }
    )");
    serverForm->addRow("服务器URL:", serverUrlEdit_);
    serverForm->addRow("访问令牌:", accessTokenEdit_);
    serverForm->addRow("设备ID:", deviceIdEdit_);
    serverForm->addRow("客户端ID:", clientIdEdit_);
    auto* serverBtnLayout = new QHBoxLayout;
    testServerButton_->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 8px;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    serverBtnLayout->addWidget(testServerButton_);
    serverBtnLayout->addWidget(statusLabel_);
    serverForm->addRow(serverBtnLayout);

    // 音频设置组
    auto* audioGroup = new QGroupBox("音频设置", this);
    audioGroup->setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            font-size: 16px;
            border: 1px solid #007bff;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: #1e1e1e;
            color: white;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
            color: #007bff;
        }
    )");
    auto* audioForm = new QFormLayout(audioGroup);
    audioForm->setSpacing(8);
    deviceCombo_->setMinimumWidth(200);
    deviceCombo_->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QComboBox:focus {
            border: 1px solid #00a0ff;
        }
    )");
    audioForm->addRow("音频设备:", deviceCombo_);
    sampleRateCombo_->addItems({"16000", "48000"});
    sampleRateCombo_->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QComboBox:focus {
            border: 1px solid #00a0ff;
        }
    )");
    audioForm->addRow("采样率:", sampleRateCombo_);
    channelsCombo_->addItems({"1", "2"});
    channelsCombo_->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QComboBox:focus {
            border: 1px solid #00a0ff;
        }
    )");
    audioForm->addRow("通道数:", channelsCombo_);
    bitDepthCombo_->addItems({"16", "24", "32"});
    bitDepthCombo_->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QComboBox:focus {
            border: 1px solid #00a0ff;
        }
    )");
    audioForm->addRow("位深度:", bitDepthCombo_);
    testButton_->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 8px;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    audioForm->addRow(testButton_);
    audioForm->addRow(waveformLabel_);

    // 将服务器设置和音频设置添加到左右栏布局
    horizontalLayout->addWidget(serverGroup);
    horizontalLayout->addWidget(audioGroup);

    mainLayout->addLayout(horizontalLayout);

    // 保存设置按钮单独放在主布局中
    saveButton_->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 8px;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    mainLayout->addWidget(saveButton_);
}

void DeviceSettings::loadSettings() {
    // TODO: 合并音频和服务器设置的加载逻辑
}

void DeviceSettings::setupConnections() {
    // TODO: 合并音频和服务器设置的信号槽
    connect(testButton_, &QPushButton::pressed, this, &DeviceSettings::onTestButtonPressed);
    connect(testButton_, &QPushButton::released, this, &DeviceSettings::onTestButtonReleased);
}

void DeviceSettings::initializeAudioDevice() {
    // 列出本机所有录音设备
    auto devices = audioDevice_->getInputDevices();
    for (const auto& device : devices) {
        deviceCombo_->addItem(QString::fromStdString(device.name));
    }
}

void DeviceSettings::onDeviceChanged(int index) { (void)index; }
void DeviceSettings::onTestRecording() {}
void DeviceSettings::onSaveSettings() {}
void DeviceSettings::onBrowseRecordingPath() {}
void DeviceSettings::onTestConnection() {}
void DeviceSettings::onAudioData(const float* data, int frames) {
    recordedAudio_.insert(recordedAudio_.end(), data, data + frames);
}
void DeviceSettings::updateButtonState(bool isRecording, bool isPlaying) { (void)isRecording; (void)isPlaying; }
void DeviceSettings::updateButtonStateServer(bool isConnected) { (void)isConnected; }

void DeviceSettings::onTestButtonPressed() {
    if (isPlaying_) return; // Prevent recording while playing

    recordedAudio_.clear();
    isRecording_ = true;
    testButton_->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: red;
            color: white;
            border: none;
            border-radius: 8px;
            min-width: 100px;
        }
        QPushButton:hover {
            background-color: #ff4d4d;
        }
        QPushButton:pressed {
            background-color: #cc0000;
        }
    )");

    // Get selected audio parameters
    int selectedDeviceIndex = deviceCombo_->currentIndex();
    if (selectedDeviceIndex < 0) {
        QMessageBox::warning(this, "错误", "请选择音频设备");
        isRecording_ = false;
        updateButtonState(isRecording_, isPlaying_); // Update button state after error
        return;
    }
    int sampleRate = sampleRateCombo_->currentText().toInt();
    // int channels = channelsCombo_->currentText().toInt();
    // int bitDepth = bitDepthCombo_->currentText().toInt(); // Not used by AudioDevice::startRecording

    auto devices = audioDevice_->getInputDevices();
    if (static_cast<size_t>(selectedDeviceIndex) >= devices.size()) {
        QMessageBox::warning(this, "错误", "选择的音频设备无效");
        isRecording_ = false;
        updateButtonState(isRecording_, isPlaying_); // Update button state after error
        return;
    }
    // int deviceId = devices[selectedDeviceIndex].id;

    audioDevice_->setSampleRate(sampleRate);
    // AudioDevice::startRecording expects a callback that takes float* data and int frames
    audioDevice_->startRecording([this](const float* data, int frames) {
        recordedAudio_.insert(recordedAudio_.end(), data, data + frames);
        qDebug() << "录音回调被调用，帧数:" << frames << ", 当前累计帧数:" << recordedAudio_.size();
    });

    updateButtonState(isRecording_, isPlaying_);
}

void DeviceSettings::onTestButtonReleased() {
    if (!isRecording_) return; // Only stop recording if currently recording

    isRecording_ = false;
    audioDevice_->stopRecording();

    qDebug() << "录音结束，累计帧数:" << recordedAudio_.size();

    if (!recordedAudio_.empty()) {
        isPlaying_ = true;
        testButton_->setStyleSheet(R"(
            QPushButton {
                padding: 8px 16px;
                background-color: gray;
                color: white;
                border: none;
                border-radius: 8px;
                min-width: 100px;
            }
            QPushButton:hover {
                background-color: #a0a0a0;
            }
            QPushButton:pressed {
                background-color: #808080;
            }
        )");
        testButton_->setEnabled(false); // 禁用按钮，防止在播放时被点击

        // 播放录音并设置播放完成的回调
        audioDevice_->playAudio(recordedAudio_.data(), recordedAudio_.size(), [this]() {
            isPlaying_ = false;
            testButton_->setEnabled(true);
            testButton_->setStyleSheet(R"(
                QPushButton {
                    padding: 8px 16px;
                    background-color: #007bff;
                    color: white;
                    border: none;
                    border-radius: 8px;
                    min-width: 100px;
                }
                QPushButton:hover {
                    background-color: #0056b3;
                }
                QPushButton:pressed {
                    background-color: #004085;
                }
            )");
        });
    }

    updateButtonState(isRecording_, isPlaying_);
}

void DeviceSettings::playbackFinished() {
    // Placeholder for playback finished callback if AudioDevice supports it
    // For now, updateButtonState is called after playAudio returns (simplistic)
}

void DeviceSettings::onBitDepthChanged(int) {}
void DeviceSettings::onChannelsChanged(int) {}
void DeviceSettings::onSampleRateChanged(int) {}
void DeviceSettings::onSaveButtonClicked() {}
void DeviceSettings::onTestButtonClicked() {}
void DeviceSettings::onTestServerButtonClicked() {}

} // namespace ui
} // namespace perfx