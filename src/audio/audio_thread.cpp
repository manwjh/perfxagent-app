#include "../../include/audio/audio_thread.h"
#include "../../include/audio/audio_device.h"
#include "../../include/audio/audio_processor.h"
#include <portaudio.h>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

namespace perfx {
namespace audio {

class AudioThread::Impl {
public:
    Impl() : stream_(nullptr), running_(false), hasInputDevice_(false), hasOutputDevice_(false),
             resamplingEnabled_(false), resamplingProcessor_(nullptr) {}

    ~Impl() {
        stop();
    }

    bool initialize(const AudioConfig& config) {
        config_ = config;
        return true;
    }

    bool start() {
        if (running_) return true;

        // 如果启用了重采样，初始化重采样处理器
        if (resamplingEnabled_ && resamplingProcessor_) {
            if (!resamplingProcessor_->startResampling(
                static_cast<SampleRate>(config_.sampleRate),
                static_cast<SampleRate>(targetSampleRate_))) {
                throw std::runtime_error("Failed to initialize resampling");
            }
        }

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
            throw std::runtime_error("Failed to open stream: " + std::string(Pa_GetErrorText(err)));
        }

        err = Pa_StartStream(stream_);
        if (err != paNoError) {
            Pa_CloseStream(stream_);
            stream_ = nullptr;
            throw std::runtime_error("Failed to start stream: " + std::string(Pa_GetErrorText(err)));
        }

        running_ = true;
        return true;
    }

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

    bool setInputDevice(const DeviceInfo& device) {
        if (running_) return false;

        inputDevice_ = device;
        inputParams_.device = device.index;
        inputParams_.channelCount = static_cast<int>(config_.channels);
        inputParams_.sampleFormat = paFloat32;
        inputParams_.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowInputLatency;
        inputParams_.hostApiSpecificStreamInfo = nullptr;
        hasInputDevice_ = true;

        return true;
    }

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

    void addProcessor(std::shared_ptr<AudioProcessor> processor) {
        processors_.push_back(processor);
    }

    void setInputCallback(AudioCallback callback) {
        inputCallback_ = std::move(callback);
    }

    bool enableResampling(SampleRate targetRate) {
        if (running_) return false;

        resamplingEnabled_ = true;
        targetSampleRate_ = static_cast<int>(targetRate);
        resamplingProcessor_ = std::make_shared<AudioProcessor>();
        return resamplingProcessor_->initialize(config_);
    }

    void disableResampling() {
        if (running_) return;

        resamplingEnabled_ = false;
        resamplingProcessor_.reset();
    }

    bool isRunning() const {
        return running_;
    }

    AudioConfig getCurrentConfig() const {
        return config_;
    }

private:
    static int audioCallback(const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* /*timeInfo*/,
                           PaStreamCallbackFlags /*statusFlags*/,
                           void* userData) {
        auto* impl = static_cast<Impl*>(userData);
        
        // 处理输入
        if (input) {
            if (impl->resamplingEnabled_ && impl->resamplingProcessor_) {
                // 分配重采样输出缓冲区
                size_t samplesPerFrame = static_cast<int>(impl->config_.channels);
                std::vector<float> resampledData(frameCount * samplesPerFrame);
                size_t outputFrames = frameCount;

                // 执行重采样
                if (impl->resamplingProcessor_->processResampling(
                    input, frameCount, resampledData.data(), outputFrames)) {
                    // 调用用户回调，传入重采样后的数据
                    if (impl->inputCallback_) {
                        impl->inputCallback_(resampledData.data(), outputFrames);
                    }
                }
            } else {
                // 直接调用用户回调
                if (impl->inputCallback_) {
                    impl->inputCallback_(input, frameCount);
                }
            }
        }

        // 处理输出
        if (output) {
            // 这里可以添加输出处理逻辑
        }

        return paContinue;
    }

    PaStream* stream_;
    bool running_;
    bool hasInputDevice_;
    bool hasOutputDevice_;
    AudioConfig config_;
    DeviceInfo inputDevice_;
    DeviceInfo outputDevice_;
    PaStreamParameters inputParams_;
    PaStreamParameters outputParams_;
    std::vector<std::shared_ptr<AudioProcessor>> processors_;
    AudioCallback inputCallback_;

    // 重采样相关
    bool resamplingEnabled_;
    int targetSampleRate_;
    std::shared_ptr<AudioProcessor> resamplingProcessor_;
};

// AudioThread implementation
AudioThread::AudioThread() : impl_(std::make_unique<Impl>()) {}
AudioThread::~AudioThread() = default;

bool AudioThread::initialize(const AudioConfig& config) { return impl_->initialize(config); }
bool AudioThread::start() { return impl_->start(); }
void AudioThread::stop() { impl_->stop(); }
bool AudioThread::setInputDevice(const DeviceInfo& device) { return impl_->setInputDevice(device); }
bool AudioThread::setOutputDevice(const DeviceInfo& device) { return impl_->setOutputDevice(device); }
void AudioThread::addProcessor(std::shared_ptr<AudioProcessor> processor) { impl_->addProcessor(processor); }
void AudioThread::setInputCallback(AudioCallback callback) { impl_->setInputCallback(std::move(callback)); }
bool AudioThread::enableResampling(SampleRate targetRate) { return impl_->enableResampling(targetRate); }
void AudioThread::disableResampling() { impl_->disableResampling(); }
bool AudioThread::isRunning() const { return impl_->isRunning(); }
AudioConfig AudioThread::getCurrentConfig() const { return impl_->getCurrentConfig(); }

} // namespace audio
} // namespace perfx 