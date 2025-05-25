#include "ui/audio_settings.h"
#include "audio/audio_config.h"
#include <cmath>
#include <QTimer>
#include <QApplication>
#include <QPainter>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sys/stat.h>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include "audio/audio_device.h"
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>

namespace perfx {
namespace ui {

AudioWaveformWidget::AudioWaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(50);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    
    // 初始化动画定时器
    animationTimer_ = new QTimer(this);
    connect(animationTimer_, &QTimer::timeout, this, [this]() {
        ripplePhase_ += RIPPLE_SPEED;
        if (ripplePhase_ >= 1.0f) {
            ripplePhase_ = 0.0f;
        }
        update();
    });
    animationTimer_->start(16);  // 约60fps
}

// 添加采样率转换函数
std::vector<float> resampleAudio(const std::vector<float>& input, int inputRate, int outputRate) {
    if (inputRate == outputRate) {
        return input;
    }

    std::vector<float> output;
    float ratio = static_cast<float>(outputRate) / inputRate;
    output.resize(static_cast<size_t>(input.size() * ratio));

    for (size_t i = 0; i < output.size(); ++i) {
        float pos = i / ratio;
        size_t index = static_cast<size_t>(pos);
        float fraction = pos - index;

        if (index + 1 < input.size()) {
            output[i] = input[index] * (1 - fraction) + input[index + 1] * fraction;
        } else {
            output[i] = input[index];
        }
    }

    return output;
}

void AudioWaveformWidget::updateWaveform(const std::vector<float>& data) {
    // 将输入数据转换为48kHz采样率
    waveformData_ = resampleAudio(data, sampleRate_, 48000);
    update();
}

void AudioWaveformWidget::setSampleRate(int sampleRate, int channels) {
    sampleRate_ = sampleRate;
    channels_ = channels;
}

void AudioWaveformWidget::setPlaybackMode(bool isPlayback) {
    // ... 原有实现 ...
}

void AudioWaveformWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (waveformData_.empty()) {
        return;
    }

    // 计算1秒对应的采样点数
    int samplesPerSecond = sampleRate_ * channels_;
    int numPoints = std::min(static_cast<int>(waveformData_.size()), samplesPerSecond);
    
    const float midY = height() / 2.0f;
    const float scale = height() * 0.8f;
    const float centerX = width() / 2.0f;
    const float maxRadius = std::min(width(), height()) / 2.0f;

    // 计算当前帧的能量值，增加放大系数
    float energy = 0.0f;
    for (int i = 0; i < numPoints; ++i) {
        energy += std::abs(waveformData_[i]);
    }
    energy = (energy / numPoints) * 5.0f;  // 增加5倍放大
    energy = std::min(energy, 1.0f);  // 限制最大值为1.0

    // 绘制波纹
    for (int i = 0; i < RIPPLE_COUNT; ++i) {
        float phase = ripplePhase_ + static_cast<float>(i) / RIPPLE_COUNT;
        if (phase >= 1.0f) phase -= 1.0f;
        
        float radius = maxRadius * phase;
        float alpha = 1.0f - phase;
        
        QPen pen(QColor(33, 150, 243, static_cast<int>(alpha * 255)));
        pen.setWidth(2);
        painter.setPen(pen);
        
        // 根据能量值调整波纹大小，增加基础大小
        float energyScale = 0.7f + energy * 0.3f;  // 基础大小0.7，最大到1.0
        float scaledRadius = radius * energyScale;
        
        painter.drawEllipse(QPointF(centerX, midY), scaledRadius, scaledRadius);
    }
}

void AudioWaveformWidget::timerEvent(QTimerEvent* event) {
    Q_UNUSED(event);
    ripplePhase_ += RIPPLE_SPEED;
    if (ripplePhase_ >= 1.0f) {
        ripplePhase_ = 0.0f;
    }
    update();
}

AudioSettingsWidget::AudioSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , deviceCombo_(nullptr)
    , sampleRateCombo_(nullptr)
    , testButton_(nullptr)
    , waveformWidget_(nullptr)
    , isRecording_(false)
    , isPlaying_(false) {
    initUI();
    initializeAudioDevice();
}

AudioSettingsWidget::~AudioSettingsWidget() {
}

void AudioSettingsWidget::initUI() {
    auto* layout = new QVBoxLayout(this);

    // 设备选择组
    auto* deviceGroup = new QGroupBox("音频设备设置", this);
    auto* deviceLayout = new QVBoxLayout(deviceGroup);

    // 设备选择下拉框
    deviceCombo_ = new QComboBox(this);
    updateDeviceList();
    connect(deviceCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioSettingsWidget::onDeviceChanged);
    deviceLayout->addWidget(new QLabel("选择音频输入设备:", this));
    deviceLayout->addWidget(deviceCombo_);

    // 采样率选择
    sampleRateCombo_ = new QComboBox(this);
    sampleRateCombo_->addItems({"8000", "16000", "44100", "48000"});
    sampleRateCombo_->setCurrentText(QString::number(perfx::AudioConfig::getInstance().getSampleRate()));
    connect(sampleRateCombo_, &QComboBox::currentTextChanged,
            this, &AudioSettingsWidget::onSampleRateChanged);
    deviceLayout->addWidget(new QLabel("采样率:", this));
    deviceLayout->addWidget(sampleRateCombo_);

    layout->addWidget(deviceGroup);

    // 音频测试组
    auto* testGroup = new QGroupBox("音频测试", this);
    auto* testLayout = new QVBoxLayout(testGroup);

    // 录音测试按钮
    testButton_ = new QPushButton("按下录音", this);
    testButton_->setCheckable(true);
    testButton_->setStyleSheet(
        "QPushButton {"
        "    background-color: #2196F3;"  // 蓝色 - 等待状态
        "    color: white;"
        "    border: none;"
        "    padding: 8px 16px;"
        "    border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1976D2;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #F44336;"  // 红色 - 录音状态
        "}"
        "QPushButton:disabled {"
        "    background-color: #9E9E9E;"  // 灰色 - 播放状态
        "}"
    );
    connect(testButton_, &QPushButton::pressed,
            this, &AudioSettingsWidget::startTestRecording);
    connect(testButton_, &QPushButton::released,
            this, &AudioSettingsWidget::stopTestRecording);
    testLayout->addWidget(testButton_);

    // 波形显示
    waveformWidget_ = new AudioWaveformWidget(this);
    waveformWidget_->setSampleRate(perfx::AudioConfig::getInstance().getSampleRate(), 
                                 perfx::AudioConfig::getInstance().getChannels());
    testLayout->addWidget(waveformWidget_);

    layout->addWidget(testGroup);
    
    // 初始化按钮状态
    updateButtonState(false, false);
}

void AudioSettingsWidget::initializeAudioDevice() {
    // 1. 设置默认设备
    if (deviceCombo_->count() > 0) {
        int defaultDeviceId = deviceCombo_->currentData().toInt();
        perfx::AudioConfig::getInstance().setInputDevice(defaultDeviceId);
        audioDevice_.setDevice(defaultDeviceId);
    }

    // 2. 设置默认采样率
    int defaultSampleRate = perfx::AudioConfig::getInstance().getSampleRate();
    audioDevice_.setSampleRate(defaultSampleRate);  // 确保设备采样率与全局配置一致
    waveformWidget_->setSampleRate(defaultSampleRate, perfx::AudioConfig::getInstance().getChannels());

    // 3. 初始化波形显示（简化逻辑，避免临时录音流）
    // 移除临时录音流，避免程序卡死
    // if (!isRecording_ && !isPlaying_) {
    //     static bool isUpdatingWaveform = false;
    //     if (!isUpdatingWaveform) {
    //         isUpdatingWaveform = true;
    //         audioDevice_.startRecording([this](const float* data, int frames) {
    //             if (!isRecording_ && !isPlaying_) {
    //                 std::vector<float> waveformData(data, data + frames);
    //                 waveformWidget_->updateWaveform(waveformData);
    //             }
    //         });
    //         // 5秒后停止波形更新
    //         QTimer::singleShot(5000, this, [this]() {
    //             audioDevice_.stopRecording();
    //             isUpdatingWaveform = false;
    //         });
    //     }
    // }

    // 4. 确保录音缓冲区为空
    recordedAudio_.clear();

    // 5. 输出当前设备信息
    auto deviceInfo = audioDevice_.getCurrentDeviceInfo();
    std::cout << "当前音频设备: " << deviceInfo.name << std::endl;
    std::cout << "采样率: " << defaultSampleRate << "Hz" << std::endl;
    std::cout << "通道数: " << deviceInfo.channels << std::endl;
}

void AudioSettingsWidget::updateDeviceList() {
    deviceCombo_->clear();
    auto devices = audioDevice_.getInputDevices();
    for (const auto& device : devices) {
        deviceCombo_->addItem(QString::fromStdString(device.name), device.id);
    }
}

void AudioSettingsWidget::onDeviceChanged(int index) {
    if (index >= 0) {
        int deviceId = deviceCombo_->currentData().toInt();
        if (audioDevice_.setDevice(deviceId)) {
            perfx::AudioConfig::getInstance().setInputDevice(deviceId);
            emit deviceChanged(deviceId);
            // 在等待状态时，使用一个临时的录音流来更新波形
            if (!isRecording_ && !isPlaying_) {
                static bool isUpdatingWaveform = false;
                if (!isUpdatingWaveform) {
                    isUpdatingWaveform = true;
                    audioDevice_.startRecording([this](const float* data, int frames) {
                        if (!isRecording_ && !isPlaying_) {
                            std::vector<float> waveformData(data, data + frames);
                            waveformWidget_->updateWaveform(waveformData);
                        }
                    });
                    // 5秒后停止波形更新
                    QTimer::singleShot(5000, this, [this]() {
                        audioDevice_.stopRecording();
                        isUpdatingWaveform = false;
                    });
                }
            }
        }
    }
}

void AudioSettingsWidget::onSampleRateChanged(const QString& rate) {
    int sampleRate = rate.toInt();
    perfx::AudioConfig::getInstance().setSampleRate(sampleRate);
    waveformWidget_->setSampleRate(sampleRate, perfx::AudioConfig::getInstance().getChannels());
    audioDevice_.setSampleRate(sampleRate);
    // 如果正在录音，需要重新启动录音
    if (isRecording_) {
        audioDevice_.stopRecording();
        QTimer::singleShot(100, this, [this]() {
            audioDevice_.startRecording([this](const float* data, int frames) {
                onAudioData(data, frames);
            });
        });
    }
}

void AudioSettingsWidget::updateButtonState(bool isRecording, bool isPlaying) {
    isRecording_ = isRecording;
    isPlaying_ = isPlaying;
    
    // 更新波形显示模式
    waveformWidget_->setPlaybackMode(isPlaying);
    
    if (isRecording) {
        testButton_->setText("录音中");
        testButton_->setStyleSheet(
            "QPushButton {"
            "    background-color: #F44336;"  // 红色 - 录音状态
            "    color: white;"
            "    border: none;"
            "    padding: 8px 16px;"
            "    border-radius: 4px;"
            "}"
        );
        testButton_->setEnabled(true);
    } else if (isPlaying) {
        testButton_->setText("播放中");
        testButton_->setStyleSheet(
            "QPushButton {"
            "    background-color: #9E9E9E;"  // 灰色 - 播放状态
            "    color: white;"
            "    border: none;"
            "    padding: 8px 16px;"
            "    border-radius: 4px;"
            "}"
        );
        testButton_->setEnabled(false);
    } else {
        testButton_->setText("按下录音");
        testButton_->setStyleSheet(
            "QPushButton {"
            "    background-color: #2196F3;"  // 蓝色 - 等待状态
            "    color: white;"
            "    border: none;"
            "    padding: 8px 16px;"
            "    border-radius: 4px;"
            "}"
            "QPushButton:hover {"
            "    background-color: #1976D2;"
            "}"
        );
        testButton_->setEnabled(true);
    }
    
    // 强制立即更新UI
    testButton_->update();
    QApplication::processEvents();
}

void AudioSettingsWidget::startTestRecording() {
    if (!isRecording_ && !isPlaying_) {
        recordedAudio_.clear();
        // 启动录音任务
        try {
            audioDevice_.startRecording([this](const float* data, int frames) {
                if (data && frames > 0) {
                    onAudioData(data, frames);
                }
            });
            updateButtonState(true, false);
            emit recordingStarted();
        } catch (const std::exception& e) {
            std::cerr << "启动录音失败: " << e.what() << std::endl;
            updateButtonState(false, false);
        }
    }
}

void AudioSettingsWidget::stopTestRecording() {
    if (isRecording_) {
        try {
            audioDevice_.stopRecording();
            if (!recordedAudio_.empty()) {
                // 仅在 DEBUG 模式下保存录音数据到临时文件
                const bool DEBUG = true;
                if (DEBUG) {
                    // 使用跨平台路径
                    #ifdef _WIN32
                        std::string tempFile = "recordings\\recording.wav";
                    #else
                        std::string tempFile = "recordings/recording.wav";
                    #endif

                    // 确保目录存在
                    #ifdef _WIN32
                        std::filesystem::create_directories("recordings");
                    #else
                        mkdir("recordings", 0755);
                    #endif

                    std::ofstream outFile(tempFile, std::ios::binary);
                    if (outFile.is_open()) {
                        // 使用全局配置获取音频参数
                        auto& config = perfx::AudioConfig::getInstance();
                        const int sampleRate = config.getSampleRate();
                        const int channels = config.getChannels();
                        const int bitsPerSample = (config.getFormat() == perfx::AudioFormat::PCM_16BIT) ? 16 :
                                                (config.getFormat() == perfx::AudioFormat::PCM_24BIT) ? 24 :
                                                (config.getFormat() == perfx::AudioFormat::PCM_32BIT) ? 32 : 32;
                        const int dataSize = recordedAudio_.size() * sizeof(short); // 16位数据大小
                        const int headerSize = 44; // WAV 文件头大小

                        // RIFF 头
                        outFile.write("RIFF", 4);
                        int fileSize = dataSize + headerSize - 8;
                        outFile.write(reinterpret_cast<const char*>(&fileSize), 4);
                        outFile.write("WAVE", 4);

                        // fmt 子块
                        outFile.write("fmt ", 4);
                        int fmtSize = 16;
                        outFile.write(reinterpret_cast<const char*>(&fmtSize), 4);
                        short audioFormat = 1; // PCM
                        outFile.write(reinterpret_cast<const char*>(&audioFormat), 2);
                        outFile.write(reinterpret_cast<const char*>(&channels), 2);
                        outFile.write(reinterpret_cast<const char*>(&sampleRate), 4);
                        int byteRate = sampleRate * channels * bitsPerSample / 8;
                        outFile.write(reinterpret_cast<const char*>(&byteRate), 4);
                        short blockAlign = channels * bitsPerSample / 8;
                        outFile.write(reinterpret_cast<const char*>(&blockAlign), 2);
                        outFile.write(reinterpret_cast<const char*>(&bitsPerSample), 2);

                        // data 子块
                        outFile.write("data", 4);
                        outFile.write(reinterpret_cast<const char*>(&dataSize), 4);

                        // 将浮点数音频数据转换为16位整数
                        std::vector<short> audioData16Bit(recordedAudio_.size());
                        for (size_t i = 0; i < recordedAudio_.size(); ++i) {
                            // 将浮点数(-1.0到1.0)转换为16位整数(-32768到32767)
                            float sample = recordedAudio_[i];
                            // 限制在-1.0到1.0范围内
                            sample = std::max(-1.0f, std::min(1.0f, sample));
                            // 转换为16位整数
                            audioData16Bit[i] = static_cast<short>(sample * 32767.0f);
                        }

                        // 写入转换后的音频数据
                        outFile.write(reinterpret_cast<const char*>(audioData16Bit.data()), dataSize);
                        outFile.close();
                        std::cout << "录音已保存到: " << tempFile << std::endl;
                        std::cout << "音频参数: " << std::endl
                                 << "  采样率: " << sampleRate << "Hz" << std::endl
                                 << "  通道数: " << channels << std::endl
                                 << "  位深度: " << bitsPerSample << "bit" << std::endl;
                    } else {
                        std::cerr << "无法保存录音文件" << std::endl;
                    }
                }
                updateButtonState(false, true);
                
                // 延迟播放，确保设备状态已完全切换
                QTimer::singleShot(100, this, [this]() {
                    try {
                        waveformWidget_->updateWaveform(recordedAudio_);
                        audioDevice_.playAudio(recordedAudio_.data(), recordedAudio_.size());
                        auto& config = perfx::AudioConfig::getInstance();
                        int playbackTime = (recordedAudio_.size() * 1000) / (config.getSampleRate() * config.getChannels());
                        
                        // 创建新的播放定时器
                        QTimer* playbackTimer = new QTimer(this);
                        connect(playbackTimer, &QTimer::timeout, this, [this, playbackTimer, playbackTime]() {
                            static int elapsedTime = 0;
                            elapsedTime += 16;
                            if (elapsedTime >= playbackTime) {
                                playbackTimer->stop();
                                playbackTimer->deleteLater();
                                elapsedTime = 0;
                                onPlaybackFinished();
                            } else {
                                float progress = static_cast<float>(elapsedTime) / playbackTime;
                                int currentFrame = static_cast<int>(progress * recordedAudio_.size());
                                int frameSize = perfx::AudioConfig::getInstance().getBufferSize();
                                std::vector<float> currentFrameData;
                                if (currentFrame + frameSize <= recordedAudio_.size()) {
                                    currentFrameData.assign(
                                        recordedAudio_.begin() + currentFrame,
                                        recordedAudio_.begin() + currentFrame + frameSize
                                    );
                                } else {
                                    currentFrameData.assign(
                                        recordedAudio_.begin() + currentFrame,
                                        recordedAudio_.end()
                                    );
                                }
                                waveformWidget_->updateWaveform(currentFrameData);
                            }
                        });
                        playbackTimer->start(16);
                    } catch (const std::exception& e) {
                        std::cerr << "播放音频失败: " << e.what() << std::endl;
                        onPlaybackFinished();
                    }
                });
            } else {
                updateButtonState(false, false);
            }
            emit recordingStopped();
        } catch (const std::exception& e) {
            std::cerr << "停止录音失败: " << e.what() << std::endl;
            updateButtonState(false, false);
        }
    }
}

void AudioSettingsWidget::onPlaybackFinished() {
    // 立即更新按钮状态为等待状态
    updateButtonState(false, false);
    // 强制立即更新UI
    testButton_->update();
    QApplication::processEvents();
}

void AudioSettingsWidget::onAudioData(const float* data, int frames) {
    if (!data || frames <= 0) return;
    
    try {
        // 保存录音数据
        recordedAudio_.insert(recordedAudio_.end(), data, data + frames);

        // 更新波形显示
        if (isRecording_) {
            std::vector<float> waveformData(data, data + frames);
            QMetaObject::invokeMethod(waveformWidget_, [this, waveformData]() {
                waveformWidget_->updateWaveform(waveformData);
            }, Qt::QueuedConnection);
        }
    } catch (const std::exception& e) {
        std::cerr << "处理音频数据失败: " << e.what() << std::endl;
    }
}

} // namespace ui
} // namespace perfx 