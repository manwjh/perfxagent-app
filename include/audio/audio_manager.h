#pragma once

#include "audio_types.h"
#include "audio_device.h"
#include "audio_processor.h"
#include "audio_thread.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace perfx {
namespace audio {

class AudioManager {
public:
    static AudioManager& getInstance();

    // 初始化管理器
    bool initialize();
    
    // 加载配置
    bool loadConfig(const std::string& configPath);
    
    // 保存配置
    bool saveConfig(const std::string& configPath);
    
    // 获取所有可用设备
    std::vector<DeviceInfo> getAvailableDevices();
    
    // 创建音频处理线程
    std::shared_ptr<AudioThread> createAudioThread(const AudioConfig& config);
    
    // 获取音频处理器
    std::shared_ptr<AudioProcessor> getProcessor();
    
    // 更新配置
    bool updateConfig(const AudioConfig& config);
    
    // 获取当前配置
    AudioConfig getCurrentConfig() const;
    
    // 清理资源
    void cleanup();

    // 文件操作相关函数
    bool writeWavFile(const void* input, size_t frames, const std::string& filename);
    bool writeOpusFile(const void* input, size_t frames, const std::string& filename);
    bool readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames);
    std::string generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels);

private:
    AudioManager();
    ~AudioManager();
    
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 