#pragma once

#include "audio_types.h"
#include "audio_device.h"
#include "audio_processor.h"
#include "audio_thread.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <QObject>
#include <QString>
#include <QVector>
#include <chrono>
#include <functional>

namespace perfx {
namespace audio {

/**
 * @brief 歌词同步格式结构体
 * 用于存储带时间戳的文本片段，支持歌词同步显示
 */
struct LyricSegment {
    std::string text;           // 文本内容
    double startTime;           // 开始时间（毫秒）
    double endTime;             // 结束时间（毫秒）
    double confidence;          // 置信度
    bool isFinal;               // 是否为最终结果
    
    LyricSegment() : startTime(0.0), endTime(0.0), confidence(0.0), isFinal(false) {}
    LyricSegment(const std::string& t, double start, double end, double conf = 0.0, bool final = false)
        : text(t), startTime(start), endTime(end), confidence(conf), isFinal(final) {}
};

/**
 * @brief 歌词同步格式管理器
 * 管理ASR转录结果的歌词同步格式转换和存储
 */
struct LyricSyncManager {
    std::vector<LyricSegment> segments;     // 歌词片段列表
    std::string fullText;                   // 完整文本
    double totalDuration;                   // 总时长（毫秒）
    mutable std::mutex mutex;               // 线程安全锁（mutable以支持const函数）
    
    LyricSyncManager() : totalDuration(0.0) {}
    
    // 清空所有数据
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        segments.clear();
        fullText.clear();
        totalDuration = 0.0;
    }
    
    // 添加歌词片段
    void addSegment(const LyricSegment& segment) {
        std::lock_guard<std::mutex> lock(mutex);
        segments.push_back(segment);
        updateFullText();
        if (segment.endTime > totalDuration) {
            totalDuration = segment.endTime;
        }
    }
    
    // 更新完整文本
    void updateFullText() {
        fullText.clear();
        for (const auto& segment : segments) {
            if (!fullText.empty()) {
                fullText += " ";
            }
            fullText += segment.text;
        }
    }
    
    // 获取指定时间点的歌词片段
    std::vector<LyricSegment> getSegmentsAtTime(double timeMs) const {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<LyricSegment> result;
        for (const auto& segment : segments) {
            if (timeMs >= segment.startTime && timeMs <= segment.endTime) {
                result.push_back(segment);
            }
        }
        return result;
    }
    
    // 获取当前播放位置的歌词
    std::string getCurrentLyric(double timeMs) const {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& segment : segments) {
            if (timeMs >= segment.startTime && timeMs <= segment.endTime) {
                return segment.text;
            }
        }
        return "";
    }
    
    // 导出为LRC格式
    std::string exportToLRC() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::string lrc;
        lrc += "[ti:ASR转录结果]\n";
        lrc += "[ar:自动语音识别]\n";
        lrc += "[al:PerfXAgent]\n";
        lrc += "[by:ASR转录]\n\n";
        
        for (const auto& segment : segments) {
            // 转换毫秒为LRC时间格式 [mm:ss.xx]
            int minutes = static_cast<int>(segment.startTime) / 60000;
            int seconds = (static_cast<int>(segment.startTime) % 60000) / 1000;
            int centiseconds = (static_cast<int>(segment.startTime) % 1000) / 10;
            
            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "[%02d:%02d.%02d]", minutes, seconds, centiseconds);
            
            lrc += timeStr + segment.text + "\n";
        }
        return lrc;
    }
    
    // 导出为JSON格式
    std::string exportToJSON() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::string json = "{\n";
        json += "  \"fullText\": \"" + fullText + "\",\n";
        json += "  \"totalDuration\": " + std::to_string(totalDuration) + ",\n";
        json += "  \"segments\": [\n";
        
        for (size_t i = 0; i < segments.size(); ++i) {
            const auto& segment = segments[i];
            json += "    {\n";
            json += "      \"text\": \"" + segment.text + "\",\n";
            json += "      \"startTime\": " + std::to_string(segment.startTime) + ",\n";
            json += "      \"endTime\": " + std::to_string(segment.endTime) + ",\n";
            json += "      \"confidence\": " + std::to_string(segment.confidence) + ",\n";
            json += "      \"isFinal\": " + std::string(segment.isFinal ? "true" : "false") + "\n";
            json += "    }";
            if (i < segments.size() - 1) {
                json += ",";
            }
            json += "\n";
        }
        
        json += "  ]\n";
        json += "}";
        return json;
    }
};

/**
 * @brief 输出设置结构
 * 定义了音频输出的相关参数
 */
struct OutputSettings {
    EncodingFormat format = EncodingFormat::WAV;  // 输出格式
    int opusFrameLength = 20;                     // Opus帧长度(ms)
    int opusBitrate = 32000;                      // Opus比特率(bps)
    int opusComplexity = 5;                       // Opus复杂度(0-10)
    int opusApplication = 2048;                   // Opus应用类型(VOIP/Audio/LowDelay)
    std::string outputFile;                       // 输出文件名
};

class AudioManager : public QObject {
    Q_OBJECT

public:
    // 录音状态枚举
    enum class RecordingState {
        IDLE,           // 空闲状态
        PREVIEWING,     // 预备/波形预览中
        RECORDING,      // 录音中
        PAUSED,         // 暂停
        STOPPING        // 正在停止
    };

    // 录音信息结构
    struct RecordingInfo {
        RecordingState state = RecordingState::IDLE;
        std::string outputFile;
        size_t recordedBytes = 0;
        size_t recordedFrames = 0;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point pauseTime;
        double totalPausedTime = 0.0;  // 总暂停时间（秒）
    };

    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }

    // 禁止拷贝和赋值
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    AudioManager();
    ~AudioManager() override;

    bool initialize(const AudioConfig& config = AudioConfig::getDefaultInputConfig());
    bool startRecording(const std::string& outputFile);
    bool stopRecording();
    bool pauseRecording();
    bool resumeRecording();
    bool updateConfig(const AudioConfig& config);
    const AudioConfig& getConfig() const;

    // 获取所有可用设备
    std::vector<DeviceInfo> getAvailableDevices();
    
    // 创建音频处理线程
    std::shared_ptr<AudioThread> createAudioThread(const AudioConfig& config);
    
    // 获取音频处理器
    std::shared_ptr<AudioProcessor> getProcessor();
    
    // 获取当前配置
    AudioConfig getCurrentConfig() const;
    
    // 清理资源
    void cleanup();

    // ============================================================================
    // 流式录音功能
    // ============================================================================
    
    // 流式录音控制
    bool startStreamRecording(const std::string& outputFile);
    bool startWritingToFile(const std::string& outputFile);
    bool startAudioStreamOnly();  // 仅启动音频流，不保存文件
    bool pauseStreamRecording();
    bool resumeStreamRecording();
    bool stopStreamRecording();
    bool stopWritingToFile();
    
    // 状态查询
    RecordingState getRecordingState() const;
    RecordingInfo getRecordingInfo() const;
    double getRecordingDuration() const;  // 返回录音时长（秒）
    size_t getRecordedBytes() const;
    
    // 波形数据获取
    QVector<float> getLatestWaveformData() const;

    // ============================================================================
    // 歌词同步功能
    // ============================================================================
    
    /**
     * @brief 获取歌词同步管理器
     * @return 歌词同步管理器引用
     */
    LyricSyncManager& getLyricSyncManager() { return lyricSyncManager_; }
    
    /**
     * @brief 从ASR结果更新歌词同步数据
     * @param asrResult ASR识别结果JSON字符串
     * @return 是否成功更新
     */
    bool updateLyricSyncFromASR(const std::string& asrResult);
    
    /**
     * @brief 添加歌词片段
     * @param segment 歌词片段
     */
    void addLyricSegment(const LyricSegment& segment);
    
    /**
     * @brief 获取指定时间点的歌词
     * @param timeMs 时间（毫秒）
     * @return 当前时间点的歌词文本
     */
    std::string getCurrentLyric(double timeMs);
    
    /**
     * @brief 获取所有歌词片段
     * @return 歌词片段列表
     */
    std::vector<LyricSegment> getAllLyricSegments() const;
    
    /**
     * @brief 获取完整文本
     * @return 完整的转录文本
     */
    std::string getFullTranscriptionText() const;
    
    /**
     * @brief 导出为LRC格式
     * @return LRC格式字符串
     */
    std::string exportLyricsToLRC() const;
    
    /**
     * @brief 导出为JSON格式
     * @return JSON格式字符串
     */
    std::string exportLyricsToJSON() const;
    
    /**
     * @brief 保存歌词到文件
     * @param filePath 文件路径
     * @param format 格式类型 ("lrc" 或 "json")
     * @return 是否保存成功
     */
    bool saveLyricsToFile(const std::string& filePath, const std::string& format = "lrc");
    
    /**
     * @brief 清空歌词数据
     */
    void clearLyrics();

    // WAV文件操作
    struct WavHeader {
        char riff[4];        // "RIFF"
        uint32_t size;       // 文件大小 - 8
        char wave[4];        // "WAVE"
        char fmt[4];         // "fmt "
        uint32_t fmtSize;    // fmt块大小
        uint16_t format;     // 1 = PCM格式
        uint16_t channels;   // 声道数
        uint32_t sampleRate; // 采样率
        uint32_t byteRate;   // 每秒字节数
        uint16_t blockAlign; // 块对齐
        uint16_t bitsPerSample; // 位深度
        char data[4];        // "data"
        uint32_t dataSize;   // 数据大小
    };

    // Opus文件操作
    struct OpusHeader {
        unsigned char header[19];  // Opus头
        uint32_t sampleRate;       // 采样率
        uint16_t channels;         // 声道数
        uint32_t dataSize;         // 数据大小
    };

    // 文件头操作
    bool writeWavHeader(const std::string& filename, const WavHeader& header);
    bool writeOpusHeader(const std::string& filename, const OpusHeader& header);
    
    // 数据写入操作
    bool writeWavData(const std::string& filename, const void* data, size_t frames, bool append = false);
    bool writeOpusData(const std::string& filename, const void* data, size_t frames, bool append = false);

    // 生成文件头
    WavHeader generateWavHeader(size_t dataSize) const;
    OpusHeader generateOpusHeader(size_t dataSize) const;

    // 完整文件写入
    bool writeWavFile(const void* input, size_t frames, const std::string& filename);
    bool writeOpusFile(const void* input, size_t frames, const std::string& filename);
    bool readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames);
    std::string generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels);

    // 获取最后一次错误信息
    std::string getLastError() const { return lastError_; }

    // 设置外部音频数据回调
    void setExternalAudioCallback(std::function<void(const void*, void*, size_t)> callback);

    // 从JSON文件加载音频配置
    bool loadAudioConfig(AudioConfig& inputConfig, OutputSettings& outputSettings, const std::string& configPath);
    
    // 保存音频配置到JSON文件
    bool saveAudioConfig(const AudioConfig& inputConfig, const OutputSettings& outputSettings, const std::string& configPath);

Q_SIGNALS:
    void outputFileInfo(const QString& info);
    void conversionProgress(int progress);
    void conversionComplete(const QString& outputFile);
    void error(const QString& errorMessage);
    void lyricUpdated(const QString& lyric, double timeMs);  // 新增：歌词更新信号
    
    // 流式录音信号
    void recordingStateChanged(RecordingState state);
    void recordingProgressUpdated(double duration, size_t bytes);
    void waveformDataUpdated(const QVector<float>& waveformData);
    void recordingCompleted(const QString& filePath);

public Q_SLOTS:
    void emitOutputFileInfo(const QString& info) { emit outputFileInfo(info); }
    void emitConversionProgress(int progress) { emit conversionProgress(progress); }
    void emitConversionComplete(const QString& outputFile) { emit conversionComplete(outputFile); }
    void emitError(const QString& errorMessage) { emit error(errorMessage); }
    void emitLyricUpdated(const QString& lyric, double timeMs) { emit lyricUpdated(lyric, timeMs); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;

    bool initialized_ = false;
    bool isRecording_ = false;
    std::unique_ptr<AudioProcessor> processor_;
    std::unique_ptr<AudioThread> audioStreamThread_;
    std::string currentOutputFile_;
    std::vector<int16_t> recordingBuffer_;
    std::unique_ptr<AudioDevice> device_;
    AudioConfig currentConfig_;
    std::string lastError_;
    std::mutex mutex_;
    
    // 歌词同步管理器
    LyricSyncManager lyricSyncManager_;
};

} // namespace audio
} // namespace perfx 