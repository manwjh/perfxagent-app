#pragma once

#include "audio_types.h"
#include <memory>
#include <vector>
#include <string>
#include <mutex>
#include <functional>

namespace perfx {
namespace audio {

class AudioDevice {
public:
    using AudioCallback = std::function<void(const void* input, void* output, size_t frameCount)>;

    AudioDevice();
    ~AudioDevice();

    // 初始化 PortAudio
    bool initialize();
    
    // 获取所有可用的音频设备
    std::vector<DeviceInfo> getAvailableDevices();
    
    // 打开输入设备
    bool openInputDevice(const DeviceInfo& device, const AudioConfig& config);
    
    // 打开输出设备
    bool openOutputDevice(const DeviceInfo& device, const AudioConfig& config);
    
    // 开始音频流
    bool startStream();
    
    // 停止音频流
    bool stopStream();
    
    // 关闭设备
    void closeDevice();
    
    // 设置音频回调函数
    void setCallback(AudioCallback callback);
    
    // 检查设备是否正在运行
    bool isStreamActive() const;
    
    // 获取当前配置
    AudioConfig getCurrentConfig() const;
    
    // 新增方法
    bool isDeviceOpen() const;
    DeviceInfo getCurrentDevice() const;
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 