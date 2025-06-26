#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QVector>
#include <memory>
#include <vector>
#include <mutex>
#include "audio/audio_manager.h"
#include "asr/asr_manager.h"

namespace perfx {
namespace logic {

// 前向声明
class RealtimeTranscriptionController;

// ============================================================================
// 实时ASR回调类
// ============================================================================

/**
 * @brief 实时ASR回调处理类
 * 
 * 处理ASR服务的回调消息，将结果转发给UI
 */
class RealtimeAsrCallback : public Asr::AsrCallback {
public:
    explicit RealtimeAsrCallback(RealtimeTranscriptionController* controller);
    ~RealtimeAsrCallback() override = default;

    // AsrCallback接口实现
    void onOpen(Asr::AsrClient* client) override;
    void onClose(Asr::AsrClient* client) override;
    void onMessage(Asr::AsrClient* client, const std::string& message) override;
    void onError(Asr::AsrClient* client, const std::string& error) override;

private:
    RealtimeTranscriptionController* controller_;
};

// ============================================================================
// 实时转录控制器主类
// ============================================================================

class RealtimeTranscriptionController : public QObject {
    Q_OBJECT

public:
    explicit RealtimeTranscriptionController(QObject* parent = nullptr);
    ~RealtimeTranscriptionController() override;

    void refreshAudioDevices();
    void selectAudioDevice(int deviceId);
    void shutdown();

    // Recording control
    bool startRecording();
    void stopRecording();
    void pauseRecording();
    void resumeRecording();

    // 新增：判断是否正在录音
    bool isRecording() const { return isRecording_; }

    // 实时ASR控制 (新增)
    void enableRealtimeAsr(bool enable);
    bool isRealtimeAsrEnabled() const { return realtimeAsrEnabled_; }
    
    // 新增：ASR线程状态判断和启动/关闭接口
    bool isAsrThreadRunning() const;
    void startAsrThread();
    void stopAsrThread();
    
    // 转录文本管理
    void setCumulativeTranscriptionText(const QString& text) { cumulativeTranscriptionText_ = text; }
    QString getCumulativeTranscriptionText() const { return cumulativeTranscriptionText_; }

signals:
    void audioDeviceListUpdated(const QStringList& deviceNames, const QList<int>& deviceIds);
    void deviceSelectionResult(bool success, const QString& message);
    void recordingStateChanged(bool isRecording, bool isPaused);
    void recordingProgressUpdated(double duration, size_t bytes);
    void waveformUpdated(const QVector<float>& waveformData);
    void transcriptionUpdated(const QString& text);
    void recordingError(const QString& errorMessage);
    
    // 实时ASR信号 (新增)
    void asrTranscriptionUpdated(const QString& text, bool isFinal);
    void asrConnectionStatusChanged(bool connected);
    void asrError(const QString& errorMessage);
    void asrUtterancesUpdated(const QList<QVariantMap>& utterances);

private slots:
    void onWaveformTimerTimeout();

private:
    void saveRecordingFiles();  // 保存录音文件和文本文件
    void onAudioData(const void* input, void* output, size_t frameCount);
    void updateWaveform();  // 更新波形数据
    double getRecordingDuration() const;  // 获取录音时长（秒）
    size_t getRecordedBytes() const;      // 获取录音字节数
    
    // 实时ASR相关方法
    void processAsrAudio(const void* data, size_t frameCount);
    void sendAsrAudioPacket(const std::vector<uint8_t>& audioData, bool isLast);

    std::unique_ptr<audio::AudioManager> audioManager_;
    QTimer* waveformTimer_;  // 波形更新定时器
    
    // 录音状态
    bool isRecording_ = false;
    bool isPaused_ = false;
    qint64 recordingStartTime_ = 0;
    QString tempWavFilePath_;  // 临时WAV文件路径

    std::vector<audio::DeviceInfo> availableDevices_;
    int selectedDeviceId_ = -1;

    // 实时ASR状态
    bool realtimeAsrEnabled_ = false;
    std::unique_ptr<Asr::AsrManager> realtimeAsrManager_;
    std::unique_ptr<RealtimeAsrCallback> realtimeAsrCallback_;
    std::mutex asrMutex_;
    std::vector<uint8_t> asrAudioBuffer_;
    size_t asrBufferSize_ = 0;
    static constexpr size_t ASR_PACKET_SIZE = 16000 * 2 * 100 / 1000; // 100ms @ 16kHz INT16
    
    // 转录内容累积
    QString cumulativeTranscriptionText_;  // 累积的转录文本
};

} // namespace logic
} // namespace perfx 