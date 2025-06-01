/**
 * @file audio_device.cpp
 * @brief 音频设备管理类的实现
 * @details 负责音频设备的初始化、打开、关闭、数据流控制等功能
 */

#include "../../include/audio/audio_device.h"
#include <portaudio.h>
#include <stdexcept>
#include <sstream>
#include <mutex>

namespace perfx {
namespace audio {

//==============================================================================
// AudioDevice::Impl 类实现
//==============================================================================

/**
 * @class AudioDevice::Impl
 * @brief AudioDevice类的具体实现类
 * @details 使用PIMPL模式隐藏实现细节，提供音频设备管理的核心功能
 */
class AudioDevice::Impl {
public:
    //--------------------------------------------------------------------------
    // 构造和析构
    //--------------------------------------------------------------------------

    /**
     * @brief 关闭音频流的辅助函数
     * @param s 要关闭的音频流指针
     */
    static void closeStream(PaStream* s) { 
        if (s) Pa_CloseStream(s); 
    }

    /**
     * @brief 构造函数
     * @details 初始化PortAudio库，如果初始化失败则抛出异常
     */
    Impl() : stream_(nullptr, closeStream), callback_(nullptr) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            lastError_ = "Failed to initialize PortAudio: " + std::string(Pa_GetErrorText(err));
            throw std::runtime_error(lastError_);
        }
    }

    /**
     * @brief 析构函数
     * @details 关闭设备并终止PortAudio
     */
    ~Impl() {
        closeDevice();
        Pa_Terminate();
    }

    //--------------------------------------------------------------------------
    // 设备管理
    //--------------------------------------------------------------------------

    /**
     * @brief 初始化音频设备
     * @return 初始化是否成功
     */
    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return true;
    }

    /**
     * @brief 获取所有可用的音频设备
     * @return 设备信息列表
     */
    std::vector<DeviceInfo> getAvailableDevices() {
        std::vector<DeviceInfo> devices;
        int numDevices = Pa_GetDeviceCount();
        
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (!deviceInfo) continue;

            DeviceInfo info;
            info.index = i;
            info.name = deviceInfo->name;
            info.maxInputChannels = deviceInfo->maxInputChannels;
            info.maxOutputChannels = deviceInfo->maxOutputChannels;
            info.defaultSampleRate = deviceInfo->defaultSampleRate;

            // 根据输入输出通道数确定设备类型
            if (info.maxInputChannels > 0 && info.maxOutputChannels > 0) {
                info.type = DeviceType::BOTH;
            } else if (info.maxInputChannels > 0) {
                info.type = DeviceType::INPUT;
            } else {
                info.type = DeviceType::OUTPUT;
            }

            devices.push_back(info);
        }
        return devices;
    }

    /**
     * @brief 获取默认输入设备
     * @return 默认输入设备信息
     * @throw std::runtime_error 如果没有找到默认输入设备
     */
    DeviceInfo getDefaultInputDevice() {
        int deviceIndex = Pa_GetDefaultInputDevice();
        if (deviceIndex == paNoDevice) {
            throw std::runtime_error("No default input device found");
        }

        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
        DeviceInfo info;
        info.index = deviceIndex;
        info.name = deviceInfo->name;
        info.type = DeviceType::INPUT;
        info.maxInputChannels = deviceInfo->maxInputChannels;
        info.maxOutputChannels = deviceInfo->maxOutputChannels;
        info.defaultSampleRate = deviceInfo->defaultSampleRate;
        return info;
    }

    /**
     * @brief 获取默认输出设备
     * @return 默认输出设备信息
     * @throw std::runtime_error 如果没有找到默认输出设备
     */
    DeviceInfo getDefaultOutputDevice() {
        int deviceIndex = Pa_GetDefaultOutputDevice();
        if (deviceIndex == paNoDevice) {
            throw std::runtime_error("No default output device found");
        }

        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
        DeviceInfo info;
        info.index = deviceIndex;
        info.name = deviceInfo->name;
        info.type = DeviceType::OUTPUT;
        info.maxInputChannels = deviceInfo->maxInputChannels;
        info.maxOutputChannels = deviceInfo->maxOutputChannels;
        info.defaultSampleRate = deviceInfo->defaultSampleRate;
        return info;
    }

    //--------------------------------------------------------------------------
    // 设备操作
    //--------------------------------------------------------------------------

    /**
     * @brief 打开输入设备
     * @param device 要打开的输入设备信息
     * @param config 音频配置
     * @return 是否成功打开设备
     */
    bool openInputDevice(const DeviceInfo& device, const AudioConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (stream_) {
                closeDevice();
            }

            // 配置输入参数
            PaStreamParameters inputParameters;
            inputParameters.device = device.index;
            inputParameters.channelCount = static_cast<int>(config.channels);
            inputParameters.sampleFormat = getPaSampleFormat(config.format);
            inputParameters.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowInputLatency;
            inputParameters.hostApiSpecificStreamInfo = nullptr;

            // 打开音频流
            PaStream* rawStream = nullptr;
            PaError err = Pa_OpenStream(&rawStream,
                                      &inputParameters,
                                      nullptr,
                                      static_cast<double>(config.sampleRate),
                                      config.framesPerBuffer,
                                      paClipOff,
                                      streamCallback,
                                      this);

            if (err != paNoError) {
                lastError_ = "Failed to open input device: " + std::string(Pa_GetErrorText(err));
                return false;
            }

            stream_.reset(rawStream);
            currentConfig_ = config;
            currentDevice_ = device;
            return true;
        } catch (const std::exception& e) {
            lastError_ = e.what();
            return false;
        }
    }

    /**
     * @brief 打开输出设备
     * @param device 要打开的输出设备信息
     * @param config 音频配置
     * @return 是否成功打开设备
     * @throw std::runtime_error 如果打开设备失败
     */
    bool openOutputDevice(const DeviceInfo& device, const AudioConfig& config) {
        if (stream_) {
            closeDevice();
        }

        // 配置输出参数
        PaStreamParameters outputParameters;
        outputParameters.device = device.index;
        outputParameters.channelCount = static_cast<int>(config.channels);
        outputParameters.sampleFormat = getPaSampleFormat(config.format);
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

        // 打开音频流
        PaStream* rawStream = nullptr;
        PaError err = Pa_OpenStream(&rawStream,
                                  nullptr,
                                  &outputParameters,
                                  static_cast<double>(config.sampleRate),
                                  config.framesPerBuffer,
                                  paClipOff,
                                  streamCallback,
                                  this);

        if (err != paNoError) {
            throw std::runtime_error("Failed to open output device: " + std::string(Pa_GetErrorText(err)));
        }

        stream_.reset(rawStream);
        currentConfig_ = config;
        return true;
    }

    /**
     * @brief 启动音频流
     * @return 是否成功启动
     */
    bool startStream() {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (!stream_) {
                lastError_ = "No active stream";
                return false;
            }
            
            PaError err = Pa_StartStream(stream_.get());
            if (err != paNoError) {
                lastError_ = "Failed to start stream: " + std::string(Pa_GetErrorText(err));
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            lastError_ = e.what();
            return false;
        }
    }

    /**
     * @brief 停止音频流
     * @return 是否成功停止
     * @throw std::runtime_error 如果停止失败
     */
    bool stopStream() {
        if (!stream_) return false;
        
        PaError err = Pa_StopStream(stream_.get());
        if (err != paNoError) {
            throw std::runtime_error("Failed to stop stream: " + std::string(Pa_GetErrorText(err)));
        }
        return true;
    }

    /**
     * @brief 关闭当前设备
     */
    void closeDevice() {
        if (stream_) {
            Pa_CloseStream(stream_.get());
            stream_.reset();
        }
    }

    //--------------------------------------------------------------------------
    // 状态查询
    //--------------------------------------------------------------------------

    /**
     * @brief 设置音频回调函数
     * @param callback 回调函数
     */
    void setCallback(AudioCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
    }

    /**
     * @brief 检查音频流是否处于活动状态
     * @return 是否活动
     */
    bool isStreamActive() const {
        return stream_ && Pa_IsStreamActive(stream_.get()) == 1;
    }

    /**
     * @brief 获取当前音频配置
     * @return 当前配置
     */
    AudioConfig getCurrentConfig() const {
        return currentConfig_;
    }

    /**
     * @brief 检查设备是否已打开
     * @return 是否已打开
     */
    bool isDeviceOpen() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_ != nullptr;
    }

    /**
     * @brief 获取当前设备信息
     * @return 当前设备信息
     */
    DeviceInfo getCurrentDevice() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentDevice_;
    }

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息
     */
    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastError_;
    }

private:
    //--------------------------------------------------------------------------
    // 私有辅助函数
    //--------------------------------------------------------------------------

    /**
     * @brief 音频流回调函数
     * @details 处理音频数据的输入输出
     */
    static int streamCallback(const void* input, void* output,
                            unsigned long frameCount,
                            [[maybe_unused]] const PaStreamCallbackTimeInfo* timeInfo,
                            [[maybe_unused]] PaStreamCallbackFlags statusFlags,
                            void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        if (impl->callback_) {
            impl->callback_(input, output, frameCount);
        }
        return paContinue;
    }

    /**
     * @brief 将SampleFormat转换为PortAudio的PaSampleFormat
     * @param format 采样格式
     * @return PortAudio采样格式
     */
    static PaSampleFormat getPaSampleFormat(SampleFormat format) {
        switch (format) {
            case SampleFormat::FLOAT32:
                return paFloat32;
            case SampleFormat::INT16:
                return paInt16;
            case SampleFormat::INT24:
                return paInt24;
            case SampleFormat::INT32:
                return paInt32;
            default:
                return paFloat32;
        }
    }

    //--------------------------------------------------------------------------
    // 成员变量
    //--------------------------------------------------------------------------

    mutable std::mutex mutex_;  // 互斥锁，保护共享资源
    std::unique_ptr<PaStream, void(*)(PaStream*)> stream_;  // 音频流指针
    AudioCallback callback_;  // 音频回调函数
    AudioConfig currentConfig_;  // 当前音频配置
    DeviceInfo currentDevice_;  // 当前设备信息
    std::string lastError_;  // 最后一次错误信息
};

//==============================================================================
// AudioDevice 类公共接口实现
//==============================================================================

AudioDevice::AudioDevice() : impl_(std::make_unique<Impl>()) {}
AudioDevice::~AudioDevice() = default;

bool AudioDevice::initialize() { return impl_->initialize(); }
std::vector<DeviceInfo> AudioDevice::getAvailableDevices() { return impl_->getAvailableDevices(); }
DeviceInfo AudioDevice::getDefaultInputDevice() { return impl_->getDefaultInputDevice(); }
DeviceInfo AudioDevice::getDefaultOutputDevice() { return impl_->getDefaultOutputDevice(); }
bool AudioDevice::openInputDevice(const DeviceInfo& device, const AudioConfig& config) { return impl_->openInputDevice(device, config); }
bool AudioDevice::openOutputDevice(const DeviceInfo& device, const AudioConfig& config) { return impl_->openOutputDevice(device, config); }
bool AudioDevice::startStream() { return impl_->startStream(); }
bool AudioDevice::stopStream() { return impl_->stopStream(); }
void AudioDevice::closeDevice() { impl_->closeDevice(); }
void AudioDevice::setCallback(AudioCallback callback) { impl_->setCallback(std::move(callback)); }
bool AudioDevice::isStreamActive() const { return impl_->isStreamActive(); }
AudioConfig AudioDevice::getCurrentConfig() const { return impl_->getCurrentConfig(); }
bool AudioDevice::isDeviceOpen() const { return impl_->isDeviceOpen(); }
DeviceInfo AudioDevice::getCurrentDevice() const { return impl_->getCurrentDevice(); }
std::string AudioDevice::getLastError() const { return impl_->getLastError(); }

//==============================================================================
// 其他相关类实现
//==============================================================================

/**
 * @class AudioProcessor
 * @brief 音频处理器类
 * @details 负责音频数据的处理
 */
class AudioProcessor {
private:
    std::mutex mutex_;  // 互斥锁，保护共享资源
    // ... 其他成员变量

public:
    /**
     * @brief 处理音频数据
     * @return 处理是否成功
     */
    bool processAudio() {
        std::lock_guard<std::mutex> lock(mutex_);
        // ... 处理逻辑
        return true;
    }
};

/**
 * @class AudioThread
 * @brief 音频线程类
 * @details 管理多个音频处理器
 */
class AudioThread {
private:
    std::vector<std::shared_ptr<AudioProcessor>> processors_;  // 音频处理器列表
    std::mutex processorsMutex_;  // 互斥锁，保护处理器列表
    // ... 其他成员变量
};

} // namespace audio
} // namespace perfx 