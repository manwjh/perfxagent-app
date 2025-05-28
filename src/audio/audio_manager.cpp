#include "audio/audio_manager.h"
#include "audio/audio_device.h"
#include "audio/audio_thread.h"
#include <portaudio.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <mutex>
#include <memory>

namespace perfx {

class AudioManager::Impl {
public:
    Impl() : state_(AudioState::IDLE) {
        Pa_Initialize();
    }

    ~Impl() {
        cleanup();
        Pa_Terminate();
    }

    bool initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != AudioState::IDLE) {
            lastError_ = "Already initialized";
            return false;
        }

        device_ = std::make_unique<AudioDevice>();
        thread_ = std::make_unique<AudioThread>();
        
        AudioThreadConfig threadConfig;
        threadConfig.sampleRate = config_.sampleRate;
        threadConfig.channels = config_.channels;
        threadConfig.bufferSize = config_.bufferSize;
        threadConfig.vadThreshold = config_.vadThreshold;

        if (!thread_->initialize(threadConfig)) {
            lastError_ = "Failed to initialize audio thread";
            return false;
        }

        return true;
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_) {
            thread_->stop();
        }
        thread_.reset();
        device_.reset();
        state_ = AudioState::IDLE;
    }

    std::vector<InputDeviceInfo> getInputDevices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return device_ ? device_->getInputDevices() : std::vector<InputDeviceInfo>();
    }

    std::vector<OutputDeviceInfo> getOutputDevices() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return device_ ? device_->getOutputDevices() : std::vector<OutputDeviceInfo>();
    }

    bool setInputDevice(int deviceId) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            lastError_ = "Device not initialized";
            return false;
        }
        return device_->setInputDevice(deviceId);
    }

    bool setOutputDevice(int deviceId) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!device_) {
            lastError_ = "Device not initialized";
            return false;
        }
        return device_->setOutputDevice(deviceId);
    }

    int getCurrentInputDevice() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return device_ ? device_->getCurrentInputDevice() : -1;
    }

    int getCurrentOutputDevice() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return device_ ? device_->getCurrentOutputDevice() : -1;
    }

    void setConfig(const AudioConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        if (thread_) {
            AudioThreadConfig threadConfig;
            threadConfig.sampleRate = config.sampleRate;
            threadConfig.channels = config.channels;
            threadConfig.bufferSize = config.bufferSize;
            threadConfig.vadThreshold = config.vadThreshold;
            thread_->initialize(threadConfig);
        }
    }

    AudioConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    bool saveConfig(const std::string& filePath) const {
        std::lock_guard<std::mutex> lock(mutex_);
        QJsonObject obj;
        obj["sampleRate"] = config_.sampleRate;
        obj["channels"] = config_.channels;
        obj["format"] = static_cast<int>(config_.format);
        obj["bufferSize"] = config_.bufferSize;
        obj["vadThreshold"] = config_.vadThreshold;
        obj["autoStartRecording"] = config_.autoStartRecording;
        obj["maxRecordingDuration"] = config_.maxRecordingDuration;

        QFile file(QString::fromStdString(filePath));
        if (!file.open(QIODevice::WriteOnly)) {
            lastError_ = "Failed to open config file for writing";
            return false;
        }

        file.write(QJsonDocument(obj).toJson());
        return true;
    }

    bool loadConfig(const std::string& filePath) {
        std::lock_guard<std::mutex> lock(mutex_);
        QFile file(QString::fromStdString(filePath));
        if (!file.open(QIODevice::ReadOnly)) {
            lastError_ = "Failed to open config file for reading";
            return false;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull()) {
            lastError_ = "Invalid JSON format in config file";
            return false;
        }

        QJsonObject obj = doc.object();
        config_.sampleRate = obj["sampleRate"].toInt();
        config_.channels = obj["channels"].toInt();
        config_.format = static_cast<AudioFormat>(obj["format"].toInt());
        config_.bufferSize = obj["bufferSize"].toInt();
        config_.vadThreshold = obj["vadThreshold"].toDouble();
        config_.autoStartRecording = obj["autoStartRecording"].toBool();
        config_.maxRecordingDuration = obj["maxRecordingDuration"].toInt();

        return true;
    }

    bool startRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != AudioState::IDLE) {
            lastError_ = "Cannot start recording in current state";
            return false;
        }

        if (!thread_) {
            lastError_ = "Audio thread not initialized";
            return false;
        }

        thread_->setVadCallback([this](bool isActive) {
            if (callbacks_.onVadStateChanged) {
                callbacks_.onVadStateChanged(isActive);
            }
        });

        if (!thread_->startListening()) {
            lastError_ = "Failed to start listening";
            return false;
        }

        state_ = AudioState::RECORDING;
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(state_);
        }

        return true;
    }

    void stopRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_) {
            thread_->stop();
        }
        state_ = AudioState::IDLE;
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(state_);
        }
    }

    bool isRecording() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == AudioState::RECORDING;
    }

    void setRecordingCallbacks(const AudioCallbacks& callbacks) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_ = callbacks;
    }

    bool playAudio(const std::vector<float>& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ != AudioState::IDLE) {
            lastError_ = "Cannot play audio in current state";
            return false;
        }

        if (!thread_) {
            lastError_ = "Audio thread not initialized";
            return false;
        }

        if (!thread_->startPlaying(data)) {
            lastError_ = "Failed to start playing";
            return false;
        }

        state_ = AudioState::PLAYING;
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(state_);
        }

        return true;
    }

    void stopPlayback() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (thread_) {
            thread_->stop();
        }
        state_ = AudioState::IDLE;
        if (callbacks_.onStateChanged) {
            callbacks_.onStateChanged(state_);
        }
    }

    bool isPlaying() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_ == AudioState::PLAYING;
    }

    AudioState getState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    std::string getLastError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastError_;
    }

private:
    mutable std::mutex mutex_;
    std::unique_ptr<AudioDevice> device_;
    std::unique_ptr<AudioThread> thread_;
    AudioConfig config_;
    AudioState state_;
    AudioCallbacks callbacks_;
    mutable std::string lastError_;
};

// AudioManager implementation
AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager() : pimpl_(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

bool AudioManager::initialize() { return pimpl_->initialize(); }
void AudioManager::cleanup() { pimpl_->cleanup(); }

std::vector<InputDeviceInfo> AudioManager::getInputDevices() const { return pimpl_->getInputDevices(); }
std::vector<OutputDeviceInfo> AudioManager::getOutputDevices() const { return pimpl_->getOutputDevices(); }
bool AudioManager::setInputDevice(int deviceId) { return pimpl_->setInputDevice(deviceId); }
bool AudioManager::setOutputDevice(int deviceId) { return pimpl_->setOutputDevice(deviceId); }
int AudioManager::getCurrentInputDevice() const { return pimpl_->getCurrentInputDevice(); }
int AudioManager::getCurrentOutputDevice() const { return pimpl_->getCurrentOutputDevice(); }

void AudioManager::setConfig(const AudioConfig& config) { pimpl_->setConfig(config); }
AudioConfig AudioManager::getConfig() const { return pimpl_->getConfig(); }
bool AudioManager::saveConfig(const std::string& filePath) const { return pimpl_->saveConfig(filePath); }
bool AudioManager::loadConfig(const std::string& filePath) { return pimpl_->loadConfig(filePath); }

bool AudioManager::startRecording() { return pimpl_->startRecording(); }
void AudioManager::stopRecording() { pimpl_->stopRecording(); }
bool AudioManager::isRecording() const { return pimpl_->isRecording(); }
void AudioManager::setRecordingCallbacks(const AudioCallbacks& callbacks) { pimpl_->setRecordingCallbacks(callbacks); }

bool AudioManager::playAudio(const std::vector<float>& data) { return pimpl_->playAudio(data); }
void AudioManager::stopPlayback() { pimpl_->stopPlayback(); }
bool AudioManager::isPlaying() const { return pimpl_->isPlaying(); }

AudioState AudioManager::getState() const { return pimpl_->getState(); }
std::string AudioManager::getLastError() const { return pimpl_->getLastError(); }

} // namespace perfx 