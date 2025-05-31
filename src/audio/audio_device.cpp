#include "../../include/audio/audio_device.h"
#include <portaudio.h>
#include <stdexcept>
#include <sstream>
#include <mutex>

namespace perfx {
namespace audio {

class AudioDevice::Impl {
public:
    static void closeStream(PaStream* s) { if (s) Pa_CloseStream(s); }

    Impl() : stream_(nullptr, closeStream), callback_(nullptr) {
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            lastError_ = "Failed to initialize PortAudio: " + std::string(Pa_GetErrorText(err));
            throw std::runtime_error(lastError_);
        }
    }

    ~Impl() {
        closeDevice();
        Pa_Terminate();
    }

    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return true;
    }

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

    bool openInputDevice(const DeviceInfo& device, const AudioConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (stream_) {
                closeDevice();
            }

            PaStreamParameters inputParameters;
            inputParameters.device = device.index;
            inputParameters.channelCount = static_cast<int>(config.channels);
            inputParameters.sampleFormat = getPaSampleFormat(config.format);
            inputParameters.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowInputLatency;
            inputParameters.hostApiSpecificStreamInfo = nullptr;

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

    bool openOutputDevice(const DeviceInfo& device, const AudioConfig& config) {
        if (stream_) {
            closeDevice();
        }

        PaStreamParameters outputParameters;
        outputParameters.device = device.index;
        outputParameters.channelCount = static_cast<int>(config.channels);
        outputParameters.sampleFormat = getPaSampleFormat(config.format);
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(device.index)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;

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

    bool stopStream() {
        if (!stream_) return false;
        
        PaError err = Pa_StopStream(stream_.get());
        if (err != paNoError) {
            throw std::runtime_error("Failed to stop stream: " + std::string(Pa_GetErrorText(err)));
        }
        return true;
    }

    void closeDevice() {
        if (stream_) {
            Pa_CloseStream(stream_.get());
            stream_.reset();
        }
    }

    void setCallback(AudioCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
    }

    bool isStreamActive() const {
        return stream_ && Pa_IsStreamActive(stream_.get()) == 1;
    }

    AudioConfig getCurrentConfig() const {
        return currentConfig_;
    }

    bool isDeviceOpen() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stream_ != nullptr;
    }

    DeviceInfo getCurrentDevice() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentDevice_;
    }

    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastError_;
    }

private:
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

    mutable std::mutex mutex_;
    std::unique_ptr<PaStream, void(*)(PaStream*)> stream_;
    AudioCallback callback_;
    AudioConfig currentConfig_;
    DeviceInfo currentDevice_;
    std::string lastError_;
};

// AudioDevice implementation
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

class AudioProcessor {
private:
    std::mutex mutex_;
    // ...
public:
    bool processAudio() {
        std::lock_guard<std::mutex> lock(mutex_);
        // ...
        return true;
    }
};

class AudioThread {
private:
    std::vector<std::shared_ptr<AudioProcessor>> processors_;
    std::mutex processorsMutex_;
    // ...
};

} // namespace audio
} // namespace perfx 