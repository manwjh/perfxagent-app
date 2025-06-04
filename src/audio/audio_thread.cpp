/**
 * @file audio_thread.cpp
 * @brief 实现音频线程的核心功能，包括音频流的初始化和处理
 */

#include "../../include/audio/audio_thread.h"
#include "../../include/audio/audio_device.h"
#include "../../include/audio/audio_processor.h"
#include <portaudio.h>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <iostream>

namespace perfx {
namespace audio {

/**
 * @class AudioThread::Impl
 * @brief AudioThread类的PIMPL实现，封装了音频处理的核心功能
 */
class AudioThread::Impl {
public:
    /**
     * @brief 构造函数，初始化音频处理相关的成员变量
     */
    Impl() : stream_(nullptr), running_(false), hasInputDevice_(false), hasOutputDevice_(false),
             resamplingEnabled_(false), resamplingProcessor_(nullptr) {}

    /**
     * @brief 析构函数，确保在对象销毁时停止音频流
     */
    ~Impl() {
        stop();
    }

    /**
     * @brief 初始化音频配置
     * @param processor 音频处理器实例
     * @return 初始化是否成功
     */
    bool initialize(AudioProcessor* processor) {
        config_ = processor->getConfig();
        return true;
    }

    /**
     * @brief 启动音频流
     * @return 启动是否成功
     * @throws std::runtime_error 当音频流启动失败时抛出异常
     */
    void start() {
        if (running_) return;

        std::cout << "[DEBUG] Starting audio thread..." << std::endl;
        std::cout << "[DEBUG] Input device: index=" << inputParams_.device 
                  << ", channels=" << inputParams_.channelCount 
                  << ", sampleRate=" << static_cast<int>(config_.sampleRate) << std::endl;

        // 如果启用了重采样，初始化重采样处理器
        if (resamplingEnabled_ && resamplingProcessor_) {
            int targetSampleRate_ = static_cast<int>(config_.sampleRate);
            if (!resamplingProcessor_->startResampling(
                static_cast<SampleRate>(config_.sampleRate),
                static_cast<SampleRate>(targetSampleRate_))) {
                throw std::runtime_error("Failed to initialize resampling");
            }
        }

        // 打开音频流
        std::cout << "[DEBUG] Opening audio stream with device index: " << inputParams_.device << std::endl;
        PaError err = Pa_OpenStream(
            &stream_,
            hasInputDevice_ ? &inputParams_ : nullptr,
            hasOutputDevice_ ? &outputParams_ : nullptr,
            static_cast<double>(config_.sampleRate),
            config_.framesPerBuffer,
            paClipOff,
            audioCallback,
            this
        );

        if (err != paNoError) {
            std::cerr << "[ERROR] Failed to open stream: " << Pa_GetErrorText(err) << std::endl;
            throw std::runtime_error("Failed to open stream: " + std::string(Pa_GetErrorText(err)));
        }

        // 启动音频流
        std::cout << "[DEBUG] Starting audio stream..." << std::endl;
        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            std::cerr << "[ERROR] Failed to start stream: " << Pa_GetErrorText(err) << std::endl;
            Pa_CloseStream(stream_);
            stream_ = nullptr;
            throw std::runtime_error("Failed to start stream: " + std::string(Pa_GetErrorText(err)));
        }

        running_ = true;
        std::cout << "[DEBUG] Audio thread started successfully" << std::endl;
    }

    /**
     * @brief 停止音频流并清理资源
     */
    void stop() {
        if (!running_) return;

        if (stream_) {
            Pa_StopStream(stream_);
            Pa_CloseStream(stream_);
            stream_ = nullptr;
        }

        // 如果启用了重采样，停止重采样处理器
        if (resamplingEnabled_ && resamplingProcessor_) {
            resamplingProcessor_->stopResampling();
        }

        running_ = false;
    }

    /**
     * @brief 设置输入设备
     * @param device 输入设备信息
     * @return 设置是否成功
     */
    bool setInputDevice(const DeviceInfo& device) {
        if (running_) return false;

        std::cout << "[DEBUG] setInputDevice: index=" << device.index 
                  << ", name=" << device.name 
                  << ", channels=" << device.maxInputChannels 
                  << ", sampleRate=" << device.defaultSampleRate << std::endl;

        // 获取PortAudio设备信息
        const PaDeviceInfo* paDeviceInfo = Pa_GetDeviceInfo(device.index);
        if (!paDeviceInfo) {
            std::cerr << "[ERROR] Invalid device index: " << device.index << std::endl;
            return false;
        }

        inputDevice_ = device;
        inputParams_.device = device.index;  // 使用原始设备索引
        inputParams_.channelCount = static_cast<int>(config_.channels);
        inputParams_.sampleFormat = paInt16;  // 使用16位整数格式
        inputParams_.suggestedLatency = paDeviceInfo->defaultLowInputLatency;
        inputParams_.hostApiSpecificStreamInfo = nullptr;
        hasInputDevice_ = true;

        std::cout << "[DEBUG] Device parameters set: index=" << inputParams_.device 
                  << ", channels=" << inputParams_.channelCount 
                  << ", latency=" << inputParams_.suggestedLatency << std::endl;

        return true;
    }

    /**
     * @brief 设置输出设备
     * @param device 输出设备信息
     * @return 设置是否成功
     */
    bool setOutputDevice(const DeviceInfo& device) {
        if (running_) return false;

        outputDevice_ = device;
        outputParams_.device = device.index;
        outputParams_.channelCount = static_cast<int>(config_.channels);
        outputParams_.sampleFormat = paFloat32;
        outputParams_.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowOutputLatency;
        outputParams_.hostApiSpecificStreamInfo = nullptr;
        hasOutputDevice_ = true;

        return true;
    }

    /**
     * @brief 添加音频处理器
     * @param processor 音频处理器实例
     */
    void addProcessor(std::shared_ptr<AudioProcessor> processor) {
        processors_.push_back(processor);
    }

    /**
     * @brief 设置输入回调函数
     * @param callback 输入回调函数
     */
    void setInputCallback(AudioCallback callback) {
        inputCallback_ = std::move(callback);
    }

    /**
     * @brief 启用重采样功能
     * @param targetRate 目标采样率
     * @return 启用是否成功
     */
    bool enableResampling(SampleRate targetRate) {
        if (running_) return false;

        resamplingEnabled_ = true;
        targetSampleRate_ = static_cast<int>(targetRate);
        resamplingProcessor_ = std::make_shared<AudioProcessor>();
        return resamplingProcessor_->initialize(config_);
    }

    /**
     * @brief 禁用重采样功能
     */
    void disableResampling() {
        if (running_) return;

        resamplingEnabled_ = false;
        resamplingProcessor_.reset();
    }

    /**
     * @brief 获取音频流运行状态
     * @return 是否正在运行
     */
    bool isRunning() const {
        return running_;
    }

    /**
     * @brief 获取当前音频配置
     * @return 当前音频配置
     */
    AudioConfig getCurrentConfig() const {
        return config_;
    }

    bool startRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isRecording_) {
            return false;
        }

        // 打印调用堆栈，辅助排查重复初始化问题
        std::cerr << "AudioThread::startRecording called" << std::endl;
        // 这里可以加入 backtrace 打印，但需要额外库支持，暂时用简单日志

        // 确保音频处理器已初始化
        if (!processor_) {
            std::cerr << "Audio processor not initialized, initializing now..." << std::endl;
            processor_ = std::make_shared<AudioProcessor>();
            if (!processor_->initialize(config_)) {
                std::cerr << "Failed to initialize audio processor" << std::endl;
                return false;
            }
        }

        // 开始录音
        isRecording_ = true;
        recordingThread_ = std::thread(&AudioThread::Impl::recordingThread, this);
        return true;
    }

    void recordingThread() {
        // 录音线程实现
        while (isRecording_) {
            // 录音逻辑
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

private:
    /**
     * @brief PortAudio回调函数，处理音频数据的输入和输出
     * @param input 输入音频数据
     * @param output 输出音频数据
     * @param frameCount 帧数
     * @param timeInfo 时间信息
     * @param statusFlags 状态标志
     * @param userData 用户数据
     * @return 回调状态
     */
    static int audioCallback(const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* /*timeInfo*/,
                           PaStreamCallbackFlags /*statusFlags*/,
                           void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        
        // 处理输入音频数据
        if (input) {
            if (impl->resamplingEnabled_ && impl->resamplingProcessor_) {
                // 分配重采样输出缓冲区
                size_t samplesPerFrame = static_cast<int>(impl->config_.channels);
                std::vector<int16_t> resampledData(frameCount * samplesPerFrame);
                size_t outputFrames = frameCount;

                // 执行重采样处理
                if (impl->resamplingProcessor_->processResampling(
                    input, frameCount, resampledData.data(), outputFrames)) {
                    // 调用用户回调，传入重采样后的数据
                    if (impl->inputCallback_) {
                        impl->inputCallback_(resampledData.data(), nullptr, outputFrames);
                    }
                }
            } else {
                // 直接调用用户回调
                if (impl->inputCallback_) {
                    impl->inputCallback_(input, nullptr, frameCount);
                }
            }
        }

        // 处理输出音频数据
        if (output) {
            // 这里可以添加输出处理逻辑
        }

        return paContinue;
    }

    // 成员变量
    PaStream* stream_;                                    ///< PortAudio流指针
    bool running_;                                        ///< 运行状态标志
    bool hasInputDevice_;                                 ///< 是否有输入设备
    bool hasOutputDevice_;                                ///< 是否有输出设备
    AudioConfig config_;                                  ///< 音频配置
    DeviceInfo inputDevice_;                              ///< 输入设备信息
    DeviceInfo outputDevice_;                             ///< 输出设备信息
    PaStreamParameters inputParams_;                      ///< 输入流参数
    PaStreamParameters outputParams_;                     ///< 输出流参数
    std::vector<std::shared_ptr<AudioProcessor>> processors_; ///< 音频处理器列表
    AudioCallback inputCallback_;                         ///< 输入回调函数

    // 重采样相关
    bool resamplingEnabled_;                              ///< 重采样启用标志
    int targetSampleRate_;                                ///< 目标采样率
    std::shared_ptr<AudioProcessor> resamplingProcessor_; ///< 重采样处理器

    std::mutex mutex_;
    bool isRecording_;
    std::thread recordingThread_;
    std::shared_ptr<AudioProcessor> processor_;
};

// AudioThread类实现
AudioThread::AudioThread() : impl_(std::make_unique<Impl>()) {}
AudioThread::~AudioThread() = default;

bool AudioThread::initialize(AudioProcessor* processor) { return impl_->initialize(processor); }
void AudioThread::start() { impl_->start(); }
void AudioThread::stop() { impl_->stop(); }
bool AudioThread::setInputDevice(const DeviceInfo& device) { return impl_->setInputDevice(device); }
bool AudioThread::setOutputDevice(const DeviceInfo& device) { return impl_->setOutputDevice(device); }
void AudioThread::addProcessor(std::shared_ptr<AudioProcessor> processor) { impl_->addProcessor(processor); }
void AudioThread::setInputCallback(AudioCallback callback) { impl_->setInputCallback(std::move(callback)); }
bool AudioThread::enableResampling(SampleRate targetRate) { return impl_->enableResampling(targetRate); }
void AudioThread::disableResampling() { impl_->disableResampling(); }
bool AudioThread::isRunning() const { return impl_->isRunning(); }
AudioConfig AudioThread::getCurrentConfig() const { return impl_->getCurrentConfig(); }

bool AudioThread::startRecording() {
    return impl_->startRecording();
}

} // namespace audio
} // namespace perfx 