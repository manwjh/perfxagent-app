/**
 * @file audio_thread.cpp
 * @brief 实现音频线程的核心功能，包括音频流的初始化和处理
 */

#include "audio/audio_thread.h"
#include "audio/audio_device.h"
#include "audio/audio_processor.h"
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
    Impl() : stream_(nullptr), running_(false), hasInputDevice_(false), hasOutputDevice_(false) {}

    /**
     * @brief 析构函数，确保在对象销毁时停止音频流
     */
    ~Impl() {
        std::cout << "[DEBUG] ~AudioThread::Impl called" << std::endl;
        try {
            stop();
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Exception in AudioThread destructor: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] Unknown exception in AudioThread destructor" << std::endl;
        }
        std::cout << "[DEBUG] ~AudioThread::Impl completed" << std::endl;
    }

    /**
     * @brief 初始化音频配置
     * @param processor 音频处理器实例
     * @return 初始化是否成功
     */
    bool initialize(AudioProcessor* processor) {
        config_ = processor->getConfig();
        
        // DEBUG: 显示从AudioProcessor获取的配置
        std::cout << "[DEBUG] AudioThread::initialize received config from AudioProcessor:" << std::endl;
        std::cout << "  - sampleRate: " << static_cast<int>(config_.sampleRate) << std::endl;
        std::cout << "  - channels: " << static_cast<int>(config_.channels) << std::endl;
        std::cout << "  - format: " << static_cast<int>(config_.format) << " (INT16=0, FLOAT32=3)" << std::endl;
        
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

        // 检查输入设备是否有效
        if (hasInputDevice_) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(inputParams_.device);
            if (!deviceInfo) {
                std::cerr << "[ERROR] Invalid input device index: " << inputParams_.device << std::endl;
                throw std::runtime_error("Invalid input device index");
            }
            std::cout << "[DEBUG] Input device info: " << deviceInfo->name << std::endl;
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
        std::cout << "[DEBUG] AudioThread::stop() called" << std::endl;
        if (!running_) return;

        std::cout << "[DEBUG] Stopping recording thread..." << std::endl;
        // 首先停止录音线程
        if (isRecording_) {
            isRecording_ = false;
            if (recordingThread_.joinable()) {
                recordingThread_.join();
                std::cout << "[DEBUG] Recording thread stopped" << std::endl;
            }
        }

        std::cout << "[DEBUG] Stopping PortAudio stream..." << std::endl;
        // 然后停止音频流
        if (stream_) {
            PaError err = Pa_StopStream(stream_);
            if (err != paNoError) {
                std::cerr << "[WARNING] Error stopping stream: " << Pa_GetErrorText(err) << std::endl;
            }
            
            err = Pa_CloseStream(stream_);
            if (err != paNoError) {
                std::cerr << "[WARNING] Error closing stream: " << Pa_GetErrorText(err) << std::endl;
            }
            
            stream_ = nullptr;
            std::cout << "[DEBUG] PortAudio stream stopped" << std::endl;
        }

        running_ = false;
        std::cout << "[DEBUG] AudioThread::stop() completed" << std::endl;
    }

    /**
     * @brief 设置输入设备
     * @param device 输入设备信息
     * @return 设置是否成功
     */
    bool setInputDevice(const DeviceInfo& device) {
        if (running_) return false;

        // DEBUG: 显示当前配置
        std::cout << "[DEBUG] AudioThread::setInputDevice called with current config:" << std::endl;
        std::cout << "  - sampleRate: " << static_cast<int>(config_.sampleRate) << std::endl;
        std::cout << "  - channels: " << static_cast<int>(config_.channels) << std::endl;
        std::cout << "  - format: " << static_cast<int>(config_.format) << " (INT16=0, FLOAT32=3)" << std::endl;

        inputDevice_ = device;
        inputParams_.device = device.index;
        inputParams_.channelCount = static_cast<int>(config_.channels);
        
        // 根据配置选择正确的采样格式
        switch (config_.format) {
            case SampleFormat::FLOAT32:
                inputParams_.sampleFormat = paFloat32;
                std::cout << "[DEBUG] Using FLOAT32 format for input" << std::endl;
                break;
            case SampleFormat::INT16:
                inputParams_.sampleFormat = paInt16;
                std::cout << "[DEBUG] Using INT16 format for input" << std::endl;
                break;
            default:
                std::cerr << "[ERROR] Unsupported sample format: " << static_cast<int>(config_.format) << std::endl;
                return false;
        }
        
        // 通用延迟优化：避免超低延迟导致的音频问题
        double defaultLatency = Pa_GetDeviceInfo(device.index)->defaultLowInputLatency;
        const double MIN_SAFE_LATENCY = 0.100;  // 提高到100ms最小安全延迟
        
        inputParams_.suggestedLatency = std::max(defaultLatency, MIN_SAFE_LATENCY);
        
        if (defaultLatency < MIN_SAFE_LATENCY) {
            std::cout << "[DEBUG] Device default latency (" << defaultLatency 
                     << "s) too low, using safe minimum: " << MIN_SAFE_LATENCY << "s" << std::endl;
        }
        inputParams_.hostApiSpecificStreamInfo = nullptr;
        hasInputDevice_ = true;

        std::cout << "[DEBUG] Device parameters set: index=" << device.index 
                 << ", channels=" << inputParams_.channelCount 
                 << ", format=" << inputParams_.sampleFormat 
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

    void setProcessor(std::shared_ptr<AudioProcessor> processor) {
        std::lock_guard<std::mutex> lock(mutex_);
        processor_ = processor;
        std::cout << "[DEBUG] AudioThread processor set: " << static_cast<void*>(processor.get()) << std::endl;
    }

    bool startRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isRecording_) {
            return false;
        }

        // 打印调用堆栈，辅助排查重复初始化问题
        std::cerr << "AudioThread::startRecording called" << std::endl;

        // 确保音频处理器已初始化
        if (!processor_) {
            std::cerr << "[ERROR] Audio processor not set! AudioThread should receive processor from AudioManager." << std::endl;
            return false;
        }

        // 开始录音
        isRecording_ = true;
        recordingThread_ = std::thread([this]() {
            while (isRecording_) {
                // 等待音频数据
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });

        return true;
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
    static int audioCallback(const void* input, void* /*output*/,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* /*timeInfo*/,
                           PaStreamCallbackFlags /*statusFlags*/,
                           void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        
        // 严格输入校验
        if (!input) {
            std::cerr << "[ERROR] Input buffer is NULL in audio callback" << std::endl;
            return paContinue;
        }
        if (frameCount == 0) {
            std::cerr << "[ERROR] Frame count is 0 in audio callback" << std::endl;
            return paContinue;
        }
        
        // 根据配置的格式检查输入数据
        bool dataValid = true;
        if (impl->config_.format == SampleFormat::FLOAT32) {
            const float* floatInput = static_cast<const float*>(input);
            for (unsigned long i = 0; i < frameCount; ++i) {
                if (!std::isfinite(floatInput[i])) {
                    std::cerr << "[ERROR] Invalid float32 data at index " << i << ": " << floatInput[i] << std::endl;
                    dataValid = false;
                    break;
                }
            }
        } else if (impl->config_.format == SampleFormat::INT16) {
            // INT16数据不需要检查NaN/inf，因为整数不会有这些值
            // INT16数据是有效的，不需要额外检查
            static int callbackCount = 0;
            static std::chrono::steady_clock::time_point lastLogTime = std::chrono::steady_clock::now();
            static const int LOG_INTERVAL_MS = 1000; // 每秒更新一次日志

            callbackCount++;
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLogTime).count();

            if (elapsed >= LOG_INTERVAL_MS) {
                std::cout << "\r[audioCallback] Processing INT16 input data, frameCount=240, total callbacks=" 
                          << callbackCount << std::flush;
                callbackCount = 0;
                lastLogTime = now;
            }
        } else {
            std::cerr << "[ERROR] Unsupported audio format in callback" << std::endl;
            dataValid = false;
        }
        
        if (!dataValid) {
            std::cerr << "[ERROR] Invalid audio data detected, skipping frame" << std::endl;
            return paContinue;
        }
        
        if (impl->inputCallback_) {
            impl->inputCallback_(input, nullptr, frameCount);
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

    std::mutex mutex_;
    bool isRecording_;
    std::thread recordingThread_;
    std::shared_ptr<AudioProcessor> processor_;
};

// AudioThread类实现
AudioThread::AudioThread() : impl_(std::make_unique<Impl>()) {}
AudioThread::~AudioThread() { std::cout << "[DEBUG] ~AudioThread called" << std::endl; }

bool AudioThread::initialize(AudioProcessor* processor) { return impl_->initialize(processor); }
void AudioThread::start() { impl_->start(); }
void AudioThread::stop() { impl_->stop(); }
bool AudioThread::setInputDevice(const DeviceInfo& device) { return impl_->setInputDevice(device); }
bool AudioThread::setOutputDevice(const DeviceInfo& device) { return impl_->setOutputDevice(device); }
void AudioThread::addProcessor(std::shared_ptr<AudioProcessor> processor) { impl_->addProcessor(processor); }
void AudioThread::setInputCallback(AudioCallback callback) { impl_->setInputCallback(std::move(callback)); }
bool AudioThread::isRunning() const { return impl_->isRunning(); }
AudioConfig AudioThread::getCurrentConfig() const { return impl_->getCurrentConfig(); }

void AudioThread::setProcessor(std::shared_ptr<AudioProcessor> processor) {
    impl_->setProcessor(processor);
}

bool AudioThread::startRecording() {
    return impl_->startRecording();
}

} // namespace audio
} // namespace perfx 