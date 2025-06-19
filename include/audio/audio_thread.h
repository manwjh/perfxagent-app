#pragma once

#include <functional>
#include "audio_types.h"
#include "audio_device.h"
#include "audio_processor.h"
#include <memory>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace perfx {
namespace audio {

class AudioProcessor;
struct DeviceInfo;
struct AudioConfig;

class AudioThread {
public:
    AudioThread();
    ~AudioThread();

    // 初始化线程
    bool initialize(AudioProcessor* processor);
    
    // 启动音频处理线程
    void start();
    
    // 停止音频处理线程
    void stop();
    
    // 设置输入设备
    bool setInputDevice(const DeviceInfo& device);
    
    // 设置输出设备
    bool setOutputDevice(const DeviceInfo& device);
    
    // 添加音频处理回调
    void addProcessor(std::shared_ptr<AudioProcessor> processor);
    
    // 设置音频处理器
    void setProcessor(std::shared_ptr<AudioProcessor> processor);
    
    // 设置回调
    void setInputCallback(AudioCallback callback);
    
    // 获取当前状态
    bool isRunning() const;
    
    // 获取当前配置
    AudioConfig getCurrentConfig() const;

    //
    bool startRecording();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 