#include "logic/realtime_transcription_controller.h"
#include "asr/asr_client.h"
#include <iostream>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <random>
#include <QFileDialog>
#include <QMessageBox>
#include <nlohmann/json.hpp>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <chrono>

namespace perfx {
namespace logic {

// Omitting the ControllerAsrCallback for now as we simulate the results

RealtimeTranscriptionController::RealtimeTranscriptionController(QWidget* parent)
    : QObject(parent)
    , parentWindow_(parent)
    , audioManager_(nullptr)
    , asrManager_(nullptr)
    , asrCallback_(nullptr)
    , waveformTimer_(nullptr)
    , isRecording_(false)
    , recordingStartTime_(0)
    , recordedBytes_(0)
    , realtimeAsrEnabled_(false)
    , tempWavFilePath_("")
{
    // 创建波形更新定时器
    waveformTimer_ = new QTimer(this);
    waveformTimer_->setInterval(50); // 50ms = 20fps
    connect(waveformTimer_, &QTimer::timeout, this, &RealtimeTranscriptionController::updateWaveform);
    std::cout << "[CTRL] RealtimeTranscriptionController: waveform timer created" << std::endl;
    
    // 初始化音频管理器
    audioManager_ = std::make_unique<audio::AudioManager>();
    
    // 初始化ASR管理器
    asrManager_ = std::make_unique<Asr::AsrManager>();
    
    // 创建ASR回调
    asrCallback_ = std::make_unique<RealtimeAsrCallback>(this);
    asrManager_->setCallback(asrCallback_.get());
    
    // 连接ASR信号
    connect(asrCallback_.get(), &RealtimeAsrCallback::transcriptionUpdated,
            this, &RealtimeTranscriptionController::onAsrTranscriptionUpdated);
    connect(asrCallback_.get(), &RealtimeAsrCallback::error,
            this, &RealtimeTranscriptionController::onAsrError);
    connect(asrCallback_.get(), &RealtimeAsrCallback::connectionStatusChanged,
            this, &RealtimeTranscriptionController::onAsrConnectionStatusChanged);
    connect(asrCallback_.get(), &RealtimeAsrCallback::utterancesUpdated,
            this, &RealtimeTranscriptionController::onAsrUtterancesUpdated);
    
    std::cout << "[CTRL] RealtimeTranscriptionController: all member variables initialized" << std::endl;
}

RealtimeTranscriptionController::~RealtimeTranscriptionController() {
    shutdown();
}

void RealtimeTranscriptionController::shutdown() {
    std::cout << "[CTRL] Controller shutdown initiated." << std::endl;
    
    // 1. 首先停止实时ASR
    if (realtimeAsrEnabled_ && asrManager_) {
        std::cout << "[ASR-THREAD] Stopping realtime ASR..." << std::endl;
        enableRealtimeAsr(false);
        std::cout << "[ASR-THREAD] Realtime ASR stopped." << std::endl;
    }
    
    // 2. 确保ASR连接被正确断开
    if (asrManager_) {
        std::cout << "[ASR-THREAD] Disconnecting ASR manager..." << std::endl;
        asrManager_->stopRecognition(); // 先停止识别
        asrManager_->disconnect();      // 再断开连接
        std::cout << "[ASR-THREAD] ASR manager disconnected." << std::endl;
    }
    
    // 3. 停止波形定时器
    if (waveformTimer_) {
        std::cout << "[CTRL] Stopping waveform timer..." << std::endl;
        waveformTimer_->stop();
        std::cout << "[CTRL] Waveform timer stopped." << std::endl;
        
        // 清理定时器
        waveformTimer_->deleteLater();
        waveformTimer_ = nullptr;
        std::cout << "[CTRL] Waveform timer cleanup completed." << std::endl;
    }
    
    // 4. 停止录音
    if (isRecording_) {
        std::cout << "[AUDIO-THREAD] Stopping active recording..." << std::endl;
        stopRecording();
        std::cout << "[AUDIO-THREAD] Recording stopped." << std::endl;
    }
    
    // 5. 清理音频管理器
    if (audioManager_) {
        std::cout << "[AUDIO-THREAD] Cleaning up audio manager..." << std::endl;
        audioManager_->stopRecording();
        audioManager_->shutdown();
        std::cout << "[AUDIO-THREAD] Audio manager cleanup completed." << std::endl;
    }
    
    // 6. 清理ASR回调
    if (asrCallback_) {
        std::cout << "[ASR-THREAD] Cleaning up ASR callback..." << std::endl;
        asrCallback_.reset();
        std::cout << "[ASR-THREAD] ASR callback cleaned up." << std::endl;
    }
    
    // 7. 清理ASR管理器
    if (asrManager_) {
        std::cout << "[ASR-THREAD] Cleaning up ASR manager..." << std::endl;
        asrManager_.reset();
        std::cout << "[ASR-THREAD] ASR manager cleaned up." << std::endl;
    }
    
    // 8. 最终清理音频管理器
    if (audioManager_) {
        std::cout << "[AUDIO-THREAD] Final cleanup of audio manager..." << std::endl;
        audioManager_.reset();
        std::cout << "[AUDIO-THREAD] Audio manager final cleanup completed." << std::endl;
    }
    
    // 9. 重置状态
    isRecording_ = false;
    recordingStartTime_ = 0;
    recordedBytes_ = 0;
    realtimeAsrEnabled_ = false;
    
    // 10. 清理设备列表
    availableDevices_.clear();
    
    std::cout << "[CTRL] Controller shutdown completed." << std::endl;
}

void RealtimeTranscriptionController::refreshAudioDevices() {
    std::cout << "[DEBUG] refreshAudioDevices() called" << std::endl;
    // 修复：如果audioManager_为nullptr，重新new一个，确保PortAudio能被初始化
    if (!audioManager_) {
        std::cout << "[DEBUG] audioManager_ is nullptr, recreating AudioManager..." << std::endl;
        audioManager_ = std::make_unique<audio::AudioManager>();
    }
    // 获取真实的音频设备列表
    if (audioManager_) {
        availableDevices_ = audioManager_->getAvailableDevices();
        QStringList deviceNames;
        QList<int> deviceIds;
        std::cout << "[DEBUG] Found " << availableDevices_.size() << " total devices" << std::endl;
        for (const auto& device : availableDevices_) {
            std::cout << "[DEBUG] Device " << device.index << ": " << device.name 
                      << " (maxInputChannels: " << device.maxInputChannels << ")" << std::endl;
            if (device.maxInputChannels > 0) {  // 只显示输入设备
                deviceNames.append(QString::fromStdString(device.name));
                deviceIds.append(device.index);
                std::cout << "[DEBUG] Added input device: " << device.index << " - " << device.name << std::endl;
            }
        }
        std::cout << "[DEBUG] Emitting audioDeviceListUpdated with " << deviceNames.size() << " devices" << std::endl;
        for (int i = 0; i < deviceNames.size(); ++i) {
            std::cout << "[DEBUG]   Device " << i << ": " << deviceNames[i].toStdString() << " (ID: " << deviceIds[i] << ")" << std::endl;
        }
        emit audioDeviceListUpdated(deviceNames, deviceIds);
    } else {
        std::cout << "[DEBUG] AudioManager not available, using fallback device list" << std::endl;
        QStringList deviceNames = {"Default - Microphone", "Line In (Audio Interface)"};
        QList<int> deviceIds = {0, 1};
        emit audioDeviceListUpdated(deviceNames, deviceIds);
    }
}

void RealtimeTranscriptionController::selectAudioDevice(int deviceId) {
    std::cout << "[DEBUG] selectAudioDevice() called with deviceId: " << deviceId << std::endl;
    std::cout << "[DEBUG] Available devices count: " << availableDevices_.size() << std::endl;
    
    // 打印所有可用设备的信息
    for (const auto& device : availableDevices_) {
        std::cout << "[DEBUG] Available device: " << device.index << " - " << device.name 
                  << " (maxInputChannels: " << device.maxInputChannels << ")" << std::endl;
    }
    
    // 验证设备ID有效性并找到设备
    const audio::DeviceInfo* selectedDevice = nullptr;
    for (const auto& device : availableDevices_) {
        if (device.index == deviceId) {
            selectedDevice = &device;
            std::cout << "[DEBUG] Found matching device: " << device.index << " - " << device.name << std::endl;
            break;
        }
    }
    
    if (!selectedDevice) {
        std::cout << "[ERROR] Invalid device ID: " << deviceId << std::endl;
        emit deviceSelectionResult(false, QString("Invalid device ID: %1").arg(deviceId));
        return;
    }
    
    // **关键修复：在切换设备前先清理之前的设备状态**
    std::cout << "[DEBUG] Cleaning up previous audio device state..." << std::endl;
    
    // 1. 停止波形更新定时器
    if (waveformTimer_) {
        waveformTimer_->stop();
        std::cout << "[DEBUG] Waveform timer stopped" << std::endl;
    }
    
    // 2. 清理之前的音频管理器状态
    if (audioManager_) {
        // 停止当前的音频流
        audioManager_->stopStreamRecording();
        std::cout << "[DEBUG] Previous audio stream stopped" << std::endl;
        
        // 清理音频管理器
        audioManager_->cleanup();
        std::cout << "[DEBUG] Audio manager cleaned up" << std::endl;
    }
    
    selectedDeviceId_ = deviceId;
    std::cout << "[DEBUG] Device selection successful: " << deviceId << ", maxInputChannels: " << selectedDevice->maxInputChannels << std::endl;

    // 配置音频设备为16K采样率，INT16位深，动态设置声道数
    audio::AudioConfig config;
    config.sampleRate = audio::SampleRate::RATE_16000;
    config.format = audio::SampleFormat::INT16;
    // 优先用单声道，否则用设备最大输入声道数
    if (selectedDevice->maxInputChannels >= 1) {
        config.channels = audio::ChannelCount::MONO;
        std::cout << "[DEBUG] Setting channels to MONO (1)" << std::endl;
    } else {
        config.channels = static_cast<audio::ChannelCount>(selectedDevice->maxInputChannels);
        std::cout << "[DEBUG] Setting channels to device max: " << selectedDevice->maxInputChannels << std::endl;
    }
    config.framesPerBuffer = 256;
    
    // 设置输入设备信息
    config.inputDevice = *selectedDevice;
    std::cout << "[DEBUG] Set inputDevice.index to: " << config.inputDevice.index << std::endl;

    std::cout << "[DEBUG] Audio config - sampleRate: 16000, format: INT16, channels: " 
              << static_cast<int>(config.channels) << ", framesPerBuffer: 256, inputDevice.index: " 
              << config.inputDevice.index << std::endl;

    // 初始化AudioManager
    if (audioManager_) {
        if (!audioManager_->initialize(config)) {
            std::cout << "[ERROR] Failed to initialize audio manager" << std::endl;
            emit deviceSelectionResult(false, "Failed to initialize audio manager");
            return;
        }
        std::cout << "[DEBUG] Audio manager initialized successfully" << std::endl;
        
        // **关键修复：设置音频数据回调，确保音频数据能传递到ASR**
        audioManager_->setExternalAudioCallback([this](const void* input, void* output, size_t frameCount) {
            this->onAudioData(input, output, frameCount);
        });
        
        // **关键修复：启动音频流来获取实时音频数据**
        std::cout << "[DEBUG] Starting audio stream..." << std::endl;
        if (!audioManager_->startStreamRecording("")) {
            std::cerr << "[ERROR] Failed to start audio stream" << std::endl;
            emit deviceSelectionResult(false, "Failed to start audio stream");
            return;
        }
        
        std::cout << "[DEBUG] Audio stream started successfully for waveform display" << std::endl;
        
        // **关键修复：启动波形更新定时器，但降低频率避免过度更新**
        waveformTimer_->setInterval(100); // 100ms = 10fps，降低更新频率
        waveformTimer_->start();
        std::cout << "[DEBUG] Waveform timer started with 100ms interval" << std::endl;
        
        emit deviceSelectionResult(true, "Device selected successfully");
    } else {
        std::cerr << "[ERROR] Audio manager not available" << std::endl;
        emit deviceSelectionResult(false, "Audio manager not available");
    }
}

void RealtimeTranscriptionController::onWaveformTimerTimeout() {
    // 波形更新定时器现在主要用于备用机制
    // 主要的波形数据更新通过 onAudioData 回调直接发送
    if (audioManager_) {
        QVector<float> waveformData = audioManager_->getLatestWaveformData();
        if (!waveformData.isEmpty()) {
            // 只有在没有直接音频数据时才使用定时器数据
            static int timerUpdateCount = 0;
            timerUpdateCount++;
            if (timerUpdateCount % 10 == 0) { // 每10次更新一次，避免过度更新
                emit waveformUpdated(waveformData);
            }
        }
    }
}

// 新增：音频数据回调函数
void RealtimeTranscriptionController::onAudioData(const void* input, void* output, size_t frameCount) {
    (void)output; // 输出未使用
    
    if (!input || frameCount == 0) {
        return;
    }
    
    // 添加调试信息，但限制频率
    static int controllerCallbackCount = 0;
    static std::chrono::steady_clock::time_point lastControllerLog = std::chrono::steady_clock::now();
    controllerCallbackCount++;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastControllerLog).count();
    
    if (elapsed >= 2000) { // 每2秒输出一次调试信息
        std::cout << "[DEBUG] Controller audio callback - callbacks: " << controllerCallbackCount 
                  << ", frameCount: " << frameCount << std::endl;
        controllerCallbackCount = 0;
        lastControllerLog = now;
    }
    
    // 将INT16音频数据转换为浮点数用于波形显示
    const int16_t* samples = static_cast<const int16_t*>(input);
    QVector<float> waveformData;
    waveformData.reserve(frameCount);
    
    for (size_t i = 0; i < frameCount; ++i) {
        // 将INT16转换为-1.0到1.0的浮点数
        float normalizedSample = static_cast<float>(samples[i]) / 32768.0f;
        waveformData.append(normalizedSample);
    }
    
    // **关键修复：直接发送波形数据到UI，不依赖定时器**
    if (!waveformData.isEmpty()) {
        emit waveformUpdated(waveformData);
    }
    
    // 处理实时ASR音频数据
    if (realtimeAsrEnabled_) {
        processAsrAudio(input, frameCount);
    }
}

bool RealtimeTranscriptionController::startRecording() {
    if (isRecording_) {
        std::cout << "[WARNING] Already recording" << std::endl;
        return false;
    }
    
    if (selectedDeviceId_ == -1) {
        std::cout << "[ERROR] No audio device selected" << std::endl;
        emit recordingError("No audio device selected");
        return false;
    }
    
    if (!audioManager_) {
        std::cout << "[ERROR] Audio manager not available" << std::endl;
        emit recordingError("Audio manager not available");
        return false;
    }
    
    // 创建临时WAV文件路径
    QString tempDir = QDir::tempPath();
    QString tempFileName = QString("temp_recording_%1.wav").arg(QDateTime::currentMSecsSinceEpoch());
    tempWavFilePath_ = tempDir + "/" + tempFileName;
    
    // 开始录音
    if (audioManager_->startRecording(tempWavFilePath_.toStdString())) {
        isRecording_ = true;
        recordingStartTime_ = QDateTime::currentMSecsSinceEpoch();
        
        // 启用实时ASR
        if (realtimeAsrEnabled_) {
            enableRealtimeAsr(true);
        }
        
        emit recordingStateChanged(true, false);
        std::cout << "[Logic] Recording started." << std::endl;
        return true;
    } else {
        std::cout << "[ERROR] Failed to start recording" << std::endl;
        emit recordingError("Failed to start recording");
        return false;
    }
}

void RealtimeTranscriptionController::stopRecording() {
    if (!isRecording_) {
        std::cout << "[WARNING] Not recording" << std::endl;
        return;
    }
    
    // 停止录音
    if (audioManager_) {
        audioManager_->stopRecording();
        
        // 停止实时ASR
        if (realtimeAsrEnabled_) {
            enableRealtimeAsr(false);
        }
        
        // 保存录音文件
        saveRecordingFiles();
    }
    
    isRecording_ = false;
    recordingStartTime_ = 0;
    
    emit recordingStateChanged(false, false);
    std::cout << "[Logic] Recording stopped." << std::endl;
}

void RealtimeTranscriptionController::pauseRecording() {
    if (!isRecording_) {
        std::cout << "[WARNING] Cannot pause recording" << std::endl;
        return;
    }
    
    if (audioManager_) {
        audioManager_->pauseRecording();
    }
    
    emit recordingStateChanged(true, true);
    std::cout << "[Logic] Recording paused." << std::endl;
}

void RealtimeTranscriptionController::resumeRecording() {
    if (!isRecording_) {
        std::cout << "[WARNING] Cannot resume recording" << std::endl;
        return;
    }
    
    if (audioManager_) {
        audioManager_->resumeRecording();
    }
    
    emit recordingStateChanged(true, false);
    std::cout << "[Logic] Recording resumed." << std::endl;
}

double RealtimeTranscriptionController::getRecordingDuration() const {
    if (!isRecording_ || recordingStartTime_ == 0) {
        return 0.0;
    }
    
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - recordingStartTime_;
    return elapsed / 1000.0;  // 转换为秒
}

size_t RealtimeTranscriptionController::getRecordedBytes() const {
    if (audioManager_) {
        return audioManager_->getRecordedBytes();
    }
    return 0;
}

void RealtimeTranscriptionController::saveRecordingFiles() {
    if (!audioManager_ || tempWavFilePath_.isEmpty()) {
        std::cout << "[WARNING] No recording to save" << std::endl;
        return;
    }
    
    // 使用QApplication::activeWindow()作为父窗口，而不是nullptr
    QWidget* parentWindow = QApplication::activeWindow();
    
    // 让用户选择保存目录
    QString saveDir = QFileDialog::getExistingDirectory(parentWindow, "选择保存目录", QDir::homePath());
    if (saveDir.isEmpty()) {
        std::cout << "[INFO] User cancelled file save" << std::endl;
        return;
    }
    
    // 创建工作目录（年月日格式）
    QDateTime now = QDateTime::currentDateTime();
    QString workDir = QString("%1/perfxagent%2")
                     .arg(saveDir)
                     .arg(now.toString("yyyyMMdd"));
    
    QDir dir;
    if (!dir.exists(workDir)) {
        if (!dir.mkpath(workDir)) {
            std::cout << "[ERROR] Failed to create work directory: " << workDir.toStdString() << std::endl;
            QMessageBox::critical(parentWindow, "错误", "无法创建工作目录");
            return;
        }
    }
    
    // 生成文件名（时分秒格式）
    QString timeStr = now.toString("HHmmss");
    QString wavFilePath = QString("%1/%2.wav").arg(workDir).arg(timeStr);
    QString txtFilePath = QString("%1/%2.txt").arg(workDir).arg(timeStr);
    
    // 复制WAV文件
    QFile tempFile(tempWavFilePath_);
    if (tempFile.exists()) {
        if (QFile::copy(tempWavFilePath_, wavFilePath)) {
            std::cout << "[INFO] WAV file saved: " << wavFilePath.toStdString() << std::endl;
        } else {
            std::cout << "[ERROR] Failed to copy WAV file" << std::endl;
            QMessageBox::critical(parentWindow, "错误", "无法复制录音文件");
            return;
        }
    } else {
        std::cout << "[ERROR] Temporary WAV file does not exist: " << tempWavFilePath_.toStdString() << std::endl;
        QMessageBox::critical(parentWindow, "错误", "临时录音文件不存在");
        return;
    }
    
    // 创建文本文件（包含转录内容）
    QFile txtFile(txtFilePath);
    if (txtFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&txtFile);
        out.setEncoding(QStringConverter::Utf8);
        out << "录音文件: " << wavFilePath << "\n";
        out << "录音时间: " << now.toString("yyyy-MM-dd HH:mm:ss") << "\n";
        out << "录音时长: " << QString::number(getRecordingDuration(), 'f', 2) << " 秒\n";
        out << "文件大小: " << QString::number(getRecordedBytes()) << " 字节\n\n";
        
        // 保存实际的转录内容
        out << "转录内容:\n";
        QString transcriptionText = getCumulativeTranscriptionText();
        if (!transcriptionText.isEmpty()) {
            out << transcriptionText;
        } else {
            out << "[无转录内容]";
        }
        
        txtFile.close();
        std::cout << "[INFO] Text file saved: " << txtFilePath.toStdString() << std::endl;
    } else {
        std::cout << "[ERROR] Failed to create text file: " << txtFilePath.toStdString() << std::endl;
        QMessageBox::warning(parentWindow, "警告", "无法创建文本文件，但录音文件已保存");
    }
    
    QMessageBox::information(parentWindow, "保存成功", 
                           QString("录音文件已保存到:\n%1\n%2").arg(wavFilePath).arg(txtFilePath));
}

// ============================================================================
// RealtimeAsrCallback 实现
// ============================================================================

RealtimeAsrCallback::RealtimeAsrCallback(RealtimeTranscriptionController* controller)
    : controller_(controller) {
    std::cout << "[DEBUG] RealtimeAsrCallback created" << std::endl;
}

void RealtimeAsrCallback::onOpen(Asr::AsrClient* client) {
    (void)client; // 未使用
    std::cout << "[DEBUG] ASR connection opened" << std::endl;
    if (controller_) {
        emit controller_->onAsrConnectionStatusChanged(true);
    }
}

void RealtimeAsrCallback::onClose(Asr::AsrClient* client) {
    (void)client; // 未使用
    std::cout << "[DEBUG] ASR connection closed" << std::endl;
    if (controller_) {
        emit controller_->onAsrConnectionStatusChanged(false);
    }
}

void RealtimeAsrCallback::onMessage(Asr::AsrClient* client, const std::string& message) {
    (void)client; // 未使用
    
//    std::cout << "[DEBUG] ASR message received: " << message << std::endl;
    
    if (!controller_) {
        return;
    }
    
    try {
        // 使用与audio_to_text_window.cpp相同的解析逻辑
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromUtf8(message.c_str()).toUtf8());
        if (!doc.isObject()) {
            std::cout << "[DEBUG] JSON解析失败，不是对象" << std::endl;
            return;
        }
        
        QJsonObject obj = doc.object();
        std::cout << "[DEBUG] JSON对象键: " << obj.keys().join(", ").toStdString() << std::endl;
        
        if (!obj.contains("result")) {
            std::cout << "[DEBUG] JSON对象不包含result字段" << std::endl;
            return;
        }

        QJsonObject resultObj = obj["result"].toObject();
        std::cout << "[DEBUG] result对象键: " << resultObj.keys().join(", ").toStdString() << std::endl;
        
        // 使用与audio_to_text_window.cpp完全相同的逻辑
        if (resultObj.contains("utterances") && resultObj["utterances"].isArray()) {
            QList<QVariantMap> utterList;
            for (const auto& utteranceVal : resultObj["utterances"].toArray()) {
                QJsonObject utterObj = utteranceVal.toObject();
                QVariantMap map;
                map["text"] = utterObj["text"].toString();
                map["definite"] = utterObj["definite"].toBool();
                map["start_time"] = utterObj["start_time"].toInt();
                map["end_time"] = utterObj["end_time"].toInt();
                // 保留 words 字段
                if (utterObj.contains("words") && utterObj["words"].isArray()) {
                    QVariantList wordList;
                    for (const auto& wordVal : utterObj["words"].toArray()) {
                        wordList.append(wordVal.toObject().toVariantMap());
                    }
                    map["words"] = wordList;
                } else {
                    map["words"] = QVariantList();
                }
                utterList.append(map);
            }
            emit controller_->onAsrUtterancesUpdated(utterList);
        } else if (resultObj.contains("text")) {
            QString text = resultObj["text"].toString();
            bool isFinal = resultObj.contains("is_final") ? resultObj.value("is_final").toBool() : false;
            
            // 如果是最终结果，累积转录文本
            if (isFinal && !text.isEmpty()) {
                controller_->setCumulativeTranscriptionText(text);
            }
            
            std::cout << "[DEBUG] 提取到text: " << text.toStdString() << ", isFinal: " << isFinal << std::endl;
            emit controller_->onAsrTranscriptionUpdated(text, isFinal);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse ASR message: " << e.what() << std::endl;
        emit controller_->onAsrError(QString("Failed to parse ASR message: %1").arg(e.what()));
    }
}

void RealtimeAsrCallback::onError(Asr::AsrClient* client, const std::string& error) {
    (void)client; // 未使用
    std::cerr << "[ERROR] ASR error: " << error << std::endl;
    if (controller_) {
        emit controller_->onAsrError(QString::fromStdString(error));
    }
}

// ============================================================================
// 实时ASR控制方法实现
// ============================================================================

void RealtimeTranscriptionController::enableRealtimeAsr(bool enable) {
    realtimeAsrEnabled_ = enable;
    std::cout << "[DEBUG] Realtime ASR enabled: " << (enable ? "true" : "false") << std::endl;
    
    if (enable && asrManager_) {
        // **关键修复：配置正确的音频格式和连接参数**
        Asr::AsrConfig asrConfig = asrManager_->getConfig();
        asrConfig.sampleRate = 16000;
        asrConfig.channels = 1;
        asrConfig.format = "pcm";  // 实时流使用pcm格式
        asrConfig.codec = "raw";   // 编码格式为raw(pcm)
        asrConfig.streaming = true;
        asrConfig.segDuration = 100;  // 100ms分包
        asrConfig.enableBusinessLog = true;  // 启用业务日志
        asrConfig.enableFlowLog = true;      // 启用流程日志
        asrManager_->setConfig(asrConfig);
        
        // **关键修复：确保先连接再启动识别**
        if (!asrManager_->connect()) {
            std::cerr << "[ERROR] Failed to connect to ASR service" << std::endl;
            emit asrError("Failed to connect to ASR service");
            return;
        }
        
        // 启动流式识别会话
        if (asrManager_->startRecognition()) {
            std::cout << "[DEBUG] ASR streaming recognition started" << std::endl;
            emit onAsrConnectionStatusChanged(true);
        } else {
            std::cerr << "[ERROR] Failed to start ASR streaming recognition" << std::endl;
            emit asrError("Failed to start ASR streaming recognition");
            asrManager_->disconnect();
        }
    } else if (!enable && asrManager_) {
        // 停止流式识别
        asrManager_->stopRecognition();
        asrManager_->disconnect();
        std::cout << "[DEBUG] ASR streaming recognition stopped" << std::endl;
        emit onAsrConnectionStatusChanged(false);
    }
}

// ============================================================================
// ASR线程状态判断和启动/关闭接口实现
// ============================================================================

bool RealtimeTranscriptionController::isAsrThreadRunning() const {
    return realtimeAsrEnabled_ && asrManager_ && asrManager_->isConnected();
}

void RealtimeTranscriptionController::startAsrThread() {
    if (!isAsrThreadRunning()) {
        enableRealtimeAsr(true);
    }
}

void RealtimeTranscriptionController::stopAsrThread() {
    if (isAsrThreadRunning()) {
        enableRealtimeAsr(false);
    }
}

// ============================================================================
// ASR音频处理方法实现
// ============================================================================

void RealtimeTranscriptionController::processAsrAudio(const void* data, size_t frameCount) {
    std::lock_guard<std::mutex> lock(asrMutex_);
    
    if (!realtimeAsrEnabled_ || !asrManager_) {
        return;
    }
    
    // 检查ASR连接状态
    if (!asrManager_->isConnected()) {
        std::cout << "[DEBUG] ASR not connected, skipping audio processing" << std::endl;
        return;
    }
    
//    std::cout << "[DEBUG] processAsrAudio called with " << frameCount << " frames" << std::endl;
    
    // 累积音频数据
    const int16_t* samples = static_cast<const int16_t*>(data);
    size_t oldSize = asrAudioBuffer_.size();
    asrAudioBuffer_.resize(oldSize + frameCount * sizeof(int16_t));
    std::memcpy(asrAudioBuffer_.data() + oldSize, samples, frameCount * sizeof(int16_t));
    asrBufferSize_ += frameCount;
    
//    std::cout << "[DEBUG] Audio buffer size: " << asrBufferSize_ << " frames, target: " << ASR_PACKET_SIZE << " frames" << std::endl;
    
    // 检查是否有完整的100ms包
    while (asrBufferSize_ >= ASR_PACKET_SIZE) {
        // 提取100ms的音频数据
        std::vector<uint8_t> audioPacket;
        audioPacket.resize(ASR_PACKET_SIZE * sizeof(int16_t));
        
        std::memcpy(audioPacket.data(), 
                   asrAudioBuffer_.data(), 
                   ASR_PACKET_SIZE * sizeof(int16_t));
        
        // 发送到ASR服务
        sendAsrAudioPacket(audioPacket, false);
        
        // 移除已发送的数据
        asrAudioBuffer_.erase(asrAudioBuffer_.begin(), 
                             asrAudioBuffer_.begin() + ASR_PACKET_SIZE * sizeof(int16_t));
        asrBufferSize_ -= ASR_PACKET_SIZE;
        
        //std::cout << "[DEBUG] Sent ASR packet, remaining buffer: " << asrBufferSize_ << " frames" << std::endl;
    }
}

void RealtimeTranscriptionController::sendAsrAudioPacket(const std::vector<uint8_t>& audioData, bool isLast) {
    if (!asrManager_) {
        return;
    }
    
    if (asrManager_->sendAudio(audioData, isLast)) {
        std::cout << "[DEBUG] ASR audio packet sent: " << audioData.size() << " bytes" << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to send ASR audio packet" << std::endl;
    }
}

} // namespace logic
} // namespace perfx

#include "realtime_transcription_controller.moc" 