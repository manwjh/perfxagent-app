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

namespace perfx {
namespace audio {

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

class AudioManager {
public:
    static AudioManager& getInstance() {
        static AudioManager instance;
        return instance;
    }

    // 禁止拷贝和赋值
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    AudioManager();
    ~AudioManager();

    bool initialize(const AudioConfig& config = AudioConfig::getDefaultInputConfig());
    bool startRecording(const std::string& outputFile);
    bool stopRecording();
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

    const VADStatus& getVADStatus() const;

    // 从JSON文件加载音频配置
    bool loadAudioConfig(AudioConfig& inputConfig, OutputSettings& outputSettings, const std::string& configPath);
    
    // 保存音频配置到JSON文件
    bool saveAudioConfig(const AudioConfig& inputConfig, const OutputSettings& outputSettings, const std::string& configPath);

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
};

} // namespace audio
} // namespace perfx 