#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>

namespace perfx {

// 音频设备类型
enum class AudioDeviceType {
    INPUT,      // 输入设备（麦克风）
    OUTPUT,     // 输出设备（扬声器）
    BOTH        // 输入输出设备
};

// 音频设备信息
struct AudioDeviceInfo {
    int id;                     // 设备ID
    std::string name;           // 设备名称
    AudioDeviceType type;       // 设备类型
    int maxChannels;            // 最大通道数
    std::vector<int> sampleRates; // 支持的采样率列表
    bool isDefault;             // 是否为默认设备
};

// 音频格式
enum class AudioFormat {
    PCM_16BIT,  // 16位PCM
    PCM_24BIT,  // 24位PCM
    PCM_32BIT,  // 32位PCM
    FLOAT32     // 32位浮点
};

// 音频配置类
class AudioConfig {
public:
    static AudioConfig& getInstance();

    // 禁止拷贝和移动
    AudioConfig(const AudioConfig&) = delete;
    AudioConfig& operator=(const AudioConfig&) = delete;
    AudioConfig(AudioConfig&&) = delete;
    AudioConfig& operator=(AudioConfig&&) = delete;

    // 设备配置
    void setInputDevice(int deviceId);
    void setOutputDevice(int deviceId);
    int getInputDevice() const;
    int getOutputDevice() const;
    std::vector<AudioDeviceInfo> getInputDevices() const;
    std::vector<AudioDeviceInfo> getOutputDevices() const;

    // 音频参数配置
    void setSampleRate(int rate);
    void setChannels(int channels);
    void setFormat(AudioFormat format);
    void setBufferSize(int size);
    int getSampleRate() const;
    int getChannels() const;
    AudioFormat getFormat() const;
    int getBufferSize() const;

    // 配置变更通知
    using ConfigChangeCallback = std::function<void()>;
    void addConfigChangeListener(ConfigChangeCallback callback);
    void removeConfigChangeListener(ConfigChangeCallback callback);

    // 配置验证
    bool validateConfig() const;
    std::string getLastError() const;

private:
    AudioConfig();
    ~AudioConfig() = default;

    void notifyConfigChanged();
    bool updateDeviceInfo();

    // 设备配置
    int inputDeviceId_;
    int outputDeviceId_;
    std::vector<AudioDeviceInfo> inputDevices_;
    std::vector<AudioDeviceInfo> outputDevices_;

    // 音频参数
    int sampleRate_;
    int channels_;
    AudioFormat format_;
    int bufferSize_;

    // 错误信息
    mutable std::string lastError_;

    // 线程安全
    mutable std::mutex mutex_;
    std::vector<ConfigChangeCallback> configChangeListeners_;
};

} // namespace perfx 