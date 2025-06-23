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

namespace perfx {
namespace logic {

// Omitting the ControllerAsrCallback for now as we simulate the results

RealtimeTranscriptionController::RealtimeTranscriptionController(QObject* parent)
    : QObject(parent), selectedDeviceId_(-1) {
    audioManager_ = std::make_unique<audio::AudioManager>();
    // No ASR Manager needed for simulation
    
    // 创建定时器来定期获取波形数据
    waveformTimer_ = new QTimer(this);
    waveformTimer_->setInterval(50); // 50ms = 20fps
    connect(waveformTimer_, &QTimer::timeout, this, &RealtimeTranscriptionController::onWaveformTimerTimeout);
    std::cout << "[DEBUG] RealtimeTranscriptionController: waveform timer created" << std::endl;

    // 初始化成员变量
    isRecording_ = false;
    isPaused_ = false;
    recordingStartTime_ = 0;
    tempWavFilePath_ = "";
    selectedDeviceId_ = -1;
    realtimeAsrEnabled_ = false;

    // 初始化实时ASR管理器
    realtimeAsrManager_ = std::make_unique<Asr::AsrManager>();
    realtimeAsrCallback_ = std::make_unique<RealtimeAsrCallback>(this);
    realtimeAsrManager_->setCallback(realtimeAsrCallback_.get());

    std::cout << "[DEBUG] RealtimeTranscriptionController: all member variables initialized" << std::endl;
}

RealtimeTranscriptionController::~RealtimeTranscriptionController() {
    shutdown();
}

void RealtimeTranscriptionController::shutdown() {
    std::cout << "[DEBUG] Controller shutdown initiated." << std::endl;
    
    // 首先停止实时ASR
    if (realtimeAsrEnabled_ && realtimeAsrManager_) {
        enableRealtimeAsr(false);
    }
    
    // 确保ASR连接被正确断开
    if (realtimeAsrManager_) {
        std::cout << "[DEBUG] Disconnecting ASR manager..." << std::endl;
        realtimeAsrManager_->disconnect();
        std::cout << "[DEBUG] ASR manager disconnected." << std::endl;
    }
    
    // 停止波形定时器
    if (waveformTimer_ && waveformTimer_->isActive()) {
        waveformTimer_->stop();
        std::cout << "[DEBUG] Waveform timer stopped." << std::endl;
    }
    
    // 清理音频管理器
    if (audioManager_) {
        audioManager_->cleanup();
        std::cout << "[DEBUG] Audio manager cleanup requested." << std::endl;
    }
    
    // 清理ASR回调
    if (realtimeAsrCallback_) {
        realtimeAsrCallback_.reset();
        std::cout << "[DEBUG] ASR callback cleaned up." << std::endl;
    }
    
    // 清理ASR管理器
    if (realtimeAsrManager_) {
        realtimeAsrManager_.reset();
        std::cout << "[DEBUG] ASR manager cleaned up." << std::endl;
    }
    
    std::cout << "[DEBUG] Controller shutdown completed." << std::endl;
}

void RealtimeTranscriptionController::refreshAudioDevices() {
    std::cout << "[DEBUG] refreshAudioDevices() called" << std::endl;
    
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
        // 模拟设备列表作为备选
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
        
        // 启动音频流来获取实时音频数据
        std::cout << "[DEBUG] Starting audio stream..." << std::endl;
        if (!audioManager_->startStreamRecording("")) {
            std::cerr << "[ERROR] Failed to start audio stream" << std::endl;
            emit deviceSelectionResult(false, "Failed to start audio stream");
            return;
        }
        
        std::cout << "[DEBUG] Audio stream started successfully for waveform display" << std::endl;
        
        // 启动波形更新定时器
        waveformTimer_->start();
        std::cout << "[DEBUG] Waveform timer started" << std::endl;
        
        emit deviceSelectionResult(true, "Device selected successfully");
    } else {
        std::cerr << "[ERROR] Audio manager not available" << std::endl;
        emit deviceSelectionResult(false, "Audio manager not available");
    }
}

void RealtimeTranscriptionController::onWaveformTimerTimeout() {
    if (audioManager_) {
        QVector<float> waveformData = audioManager_->getLatestWaveformData();
        if (!waveformData.isEmpty()) {
            emit waveformUpdated(waveformData);
        }
    }
}

// 新增：音频数据回调函数
void RealtimeTranscriptionController::onAudioData(const void* input, void* output, size_t frameCount) {
    (void)output; // 输出未使用
    
    if (!input || frameCount == 0) {
        return;
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
    
    // 发送波形数据到UI
    emit waveformUpdated(waveformData);
    
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
        isPaused_ = false;
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
    isPaused_ = false;
    recordingStartTime_ = 0;
    
    emit recordingStateChanged(false, false);
    std::cout << "[Logic] Recording stopped." << std::endl;
}

void RealtimeTranscriptionController::pauseRecording() {
    if (!isRecording_ || isPaused_) {
        std::cout << "[WARNING] Cannot pause recording" << std::endl;
        return;
    }
    
    if (audioManager_) {
        audioManager_->pauseRecording();
    }
    
    isPaused_ = true;
    emit recordingStateChanged(true, true);
    std::cout << "[Logic] Recording paused." << std::endl;
}

void RealtimeTranscriptionController::resumeRecording() {
    if (!isRecording_ || !isPaused_) {
        std::cout << "[WARNING] Cannot resume recording" << std::endl;
        return;
    }
    
    if (audioManager_) {
        audioManager_->resumeRecording();
    }
    
    isPaused_ = false;
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
        emit controller_->asrConnectionStatusChanged(true);
    }
}

void RealtimeAsrCallback::onClose(Asr::AsrClient* client) {
    (void)client; // 未使用
    std::cout << "[DEBUG] ASR connection closed" << std::endl;
    if (controller_) {
        emit controller_->asrConnectionStatusChanged(false);
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
            emit controller_->asrUtterancesUpdated(utterList);
        } else if (resultObj.contains("text")) {
            QString text = resultObj["text"].toString();
            bool isFinal = resultObj.contains("is_final") ? resultObj.value("is_final").toBool() : false;
            
            // 如果是最终结果，累积转录文本
            if (isFinal && !text.isEmpty()) {
                controller_->setCumulativeTranscriptionText(text);
            }
            
            std::cout << "[DEBUG] 提取到text: " << text.toStdString() << ", isFinal: " << isFinal << std::endl;
            emit controller_->asrTranscriptionUpdated(text, isFinal);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to parse ASR message: " << e.what() << std::endl;
        emit controller_->asrError(QString("Failed to parse ASR message: %1").arg(e.what()));
    }
}

void RealtimeAsrCallback::onError(Asr::AsrClient* client, const std::string& error) {
    (void)client; // 未使用
    std::cerr << "[ERROR] ASR error: " << error << std::endl;
    if (controller_) {
        emit controller_->asrError(QString::fromStdString(error));
    }
}

// ============================================================================
// 实时ASR控制方法实现
// ============================================================================

void RealtimeTranscriptionController::enableRealtimeAsr(bool enable) {
    realtimeAsrEnabled_ = enable;
    std::cout << "[DEBUG] Realtime ASR enabled: " << (enable ? "true" : "false") << std::endl;
    
    if (enable && realtimeAsrManager_) {
        // **关键修复：配置正确的音频格式和连接参数**
        Asr::AsrConfig asrConfig = realtimeAsrManager_->getConfig();
        asrConfig.sampleRate = 16000;
        asrConfig.channels = 1;
        asrConfig.format = "pcm";  // 实时流使用pcm格式
        asrConfig.codec = "raw";   // 编码格式为raw(pcm)
        asrConfig.streaming = true;
        asrConfig.segDuration = 100;  // 100ms分包
        asrConfig.enableBusinessLog = true;  // 启用业务日志
        asrConfig.enableFlowLog = true;      // 启用流程日志
        realtimeAsrManager_->setConfig(asrConfig);
        
        // **关键修复：确保先连接再启动识别**
        if (!realtimeAsrManager_->connect()) {
            std::cerr << "[ERROR] Failed to connect to ASR service" << std::endl;
            emit asrError("Failed to connect to ASR service");
            return;
        }
        
        // 启动流式识别会话
        if (realtimeAsrManager_->startRecognition()) {
            std::cout << "[DEBUG] ASR streaming recognition started" << std::endl;
            emit asrConnectionStatusChanged(true);
        } else {
            std::cerr << "[ERROR] Failed to start ASR streaming recognition" << std::endl;
            emit asrError("Failed to start ASR streaming recognition");
            realtimeAsrManager_->disconnect();
        }
    } else if (!enable && realtimeAsrManager_) {
        // 停止流式识别
        realtimeAsrManager_->stopRecognition();
        realtimeAsrManager_->disconnect();
        std::cout << "[DEBUG] ASR streaming recognition stopped" << std::endl;
        emit asrConnectionStatusChanged(false);
    }
}

// ============================================================================
// ASR音频处理方法实现
// ============================================================================

void RealtimeTranscriptionController::processAsrAudio(const void* data, size_t frameCount) {
    std::lock_guard<std::mutex> lock(asrMutex_);
    
    if (!realtimeAsrEnabled_ || !realtimeAsrManager_) {
        return;
    }
    
    // 检查ASR连接状态
    if (!realtimeAsrManager_->isConnected()) {
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
    if (!realtimeAsrManager_) {
        return;
    }
    
    if (realtimeAsrManager_->sendAudio(audioData, isLast)) {
        std::cout << "[DEBUG] ASR audio packet sent: " << audioData.size() << " bytes" << std::endl;
    } else {
        std::cerr << "[ERROR] Failed to send ASR audio packet" << std::endl;
    }
}

} // namespace logic
} // namespace perfx

#include "realtime_transcription_controller.moc" 