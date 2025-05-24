#include "audio/audio_config.h"
#include <algorithm>
#include <stdexcept>

namespace perfx {

AudioConfig& AudioConfig::getInstance() {
    static AudioConfig instance;
    return instance;
}

AudioConfig::AudioConfig()
    : inputDeviceId_(-1)
    , outputDeviceId_(-1)
    , sampleRate_(16000)
    , channels_(1)
    , format_(AudioFormat::PCM_16BIT)
    , bufferSize_(1024) {
    updateDeviceInfo();
}

void AudioConfig::setInputDevice(int deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deviceId != inputDeviceId_) {
        inputDeviceId_ = deviceId;
        notifyConfigChanged();
    }
}

void AudioConfig::setOutputDevice(int deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (deviceId != outputDeviceId_) {
        outputDeviceId_ = deviceId;
        notifyConfigChanged();
    }
}

int AudioConfig::getInputDevice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inputDeviceId_;
}

int AudioConfig::getOutputDevice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return outputDeviceId_;
}

std::vector<AudioDeviceInfo> AudioConfig::getInputDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return inputDevices_;
}

std::vector<AudioDeviceInfo> AudioConfig::getOutputDevices() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return outputDevices_;
}

void AudioConfig::setSampleRate(int rate) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (rate != sampleRate_) {
        sampleRate_ = rate;
        notifyConfigChanged();
    }
}

void AudioConfig::setChannels(int channels) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (channels != channels_) {
        channels_ = channels;
        notifyConfigChanged();
    }
}

void AudioConfig::setFormat(AudioFormat format) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (format != format_) {
        format_ = format;
        notifyConfigChanged();
    }
}

void AudioConfig::setBufferSize(int size) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (size != bufferSize_) {
        bufferSize_ = size;
        notifyConfigChanged();
    }
}

int AudioConfig::getSampleRate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sampleRate_;
}

int AudioConfig::getChannels() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return channels_;
}

AudioFormat AudioConfig::getFormat() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return format_;
}

int AudioConfig::getBufferSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bufferSize_;
}

void AudioConfig::addConfigChangeListener(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    configChangeListeners_.push_back(callback);
}

void AudioConfig::removeConfigChangeListener(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    // 使用函数指针来标识回调函数
    void* target = callback.target<void(*)()>();
    if (target) {
        configChangeListeners_.erase(
            std::remove_if(configChangeListeners_.begin(), configChangeListeners_.end(),
                [target](const ConfigChangeCallback& cb) {
                    return cb.target<void(*)()>() == target;
                }
            ),
            configChangeListeners_.end()
        );
    }
}

bool AudioConfig::validateConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();

    // 验证采样率
    if (sampleRate_ <= 0) {
        lastError_ = "Invalid sample rate";
        return false;
    }

    // 验证通道数
    if (channels_ <= 0 || channels_ > 8) {
        lastError_ = "Invalid number of channels";
        return false;
    }

    // 验证缓冲区大小
    if (bufferSize_ <= 0) {
        lastError_ = "Invalid buffer size";
        return false;
    }

    // 验证设备ID
    if (inputDeviceId_ < 0) {
        lastError_ = "Invalid input device ID";
        return false;
    }

    if (outputDeviceId_ < 0) {
        lastError_ = "Invalid output device ID";
        return false;
    }

    return true;
}

std::string AudioConfig::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void AudioConfig::notifyConfigChanged() {
    for (const auto& callback : configChangeListeners_) {
        try {
            callback();
        } catch (const std::exception& e) {
            lastError_ = e.what();
        }
    }
}

bool AudioConfig::updateDeviceInfo() {
    // TODO: 实现设备信息更新逻辑
    // 这里需要调用 PortAudio 或其他音频库的 API 来获取设备信息
    return true;
}

} // namespace perfx 