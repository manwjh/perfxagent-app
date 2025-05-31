#pragma once

#include "audio_types.h"
#include "audio_device.h"
#include "audio_processor.h"
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace perfx {
namespace audio {

class AudioProcessor;

class AudioThread {
public:
    using AudioCallback = std::function<void(const void* input, size_t frames)>;

    AudioThread();
    ~AudioThread();

    // 初始化线程
    bool initialize(const AudioConfig& config);
    
    // 启动音频处理线程
    bool start();
    
    // 停止音频处理线程
    void stop();
    
    // 设置输入设备
    bool setInputDevice(const DeviceInfo& device);
    
    // 设置输出设备
    bool setOutputDevice(const DeviceInfo& device);
    
    // 添加音频处理回调
    void addProcessor(std::shared_ptr<AudioProcessor> processor);
    
    // 设置回调
    void setInputCallback(AudioCallback callback);
    
    // 获取当前状态
    bool isRunning() const;
    
    // 获取当前配置
    AudioConfig getCurrentConfig() const;

    // 重采样相关函数
    bool enableResampling(SampleRate targetRate);
    void disableResampling();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 