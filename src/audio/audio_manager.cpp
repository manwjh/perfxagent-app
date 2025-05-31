#include "../../include/audio/audio_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace perfx {
namespace audio {

class AudioManager::Impl {
public:
    Impl() : initialized_(false) {}

    ~Impl() {
        cleanup();
    }

    bool initialize() {
        if (initialized_) return true;

        // 初始化音频设备（这会初始化 PortAudio）
        device_ = std::make_unique<AudioDevice>();
        if (!device_->initialize()) {
            return false;
        }

        // 初始化音频处理器，使用默认配置
        processor_ = std::make_shared<AudioProcessor>();
        AudioConfig defaultConfig;
        defaultConfig.sampleRate = SampleRate::RATE_48000;
        defaultConfig.channels = ChannelCount::MONO;
        defaultConfig.format = SampleFormat::FLOAT32;
        defaultConfig.framesPerBuffer = 256;
        
        if (!processor_->initialize(defaultConfig)) {
            return false;
        }

        initialized_ = true;
        return true;
    }

    bool loadConfig(const std::string& configPath) {
        try {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            nlohmann::json j;
            file >> j;

            AudioConfig config;
            config.sampleRate = static_cast<SampleRate>(j["sampleRate"].get<int>());
            config.channels = static_cast<ChannelCount>(j["channels"].get<int>());
            config.format = static_cast<SampleFormat>(j["format"].get<int>());
            config.framesPerBuffer = j["framesPerBuffer"].get<int>();

            return updateConfig(config);
        } catch (const std::exception&) {
            return false;
        }
    }

    bool saveConfig(const std::string& configPath) {
        try {
            nlohmann::json j;
            j["sampleRate"] = static_cast<int>(config_.sampleRate);
            j["channels"] = static_cast<int>(config_.channels);
            j["format"] = static_cast<int>(config_.format);
            j["framesPerBuffer"] = config_.framesPerBuffer;

            std::ofstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            file << j.dump(4);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::vector<DeviceInfo> getAvailableDevices() {
        return device_->getAvailableDevices();
    }

    std::shared_ptr<AudioThread> createAudioThread(const AudioConfig& config) {
        auto thread = std::make_shared<AudioThread>();
        if (!thread->initialize(config)) {
            return nullptr;
        }
        return thread;
    }

    std::shared_ptr<AudioProcessor> getProcessor() {
        return processor_;
    }

    bool updateConfig(const AudioConfig& config) {
        if (!initialized_) {
            return false;
        }

        config_ = config;
        return processor_->initialize(config);
    }

    AudioConfig getCurrentConfig() const {
        return config_;
    }

    void cleanup() {
        processor_.reset();
        device_.reset();
        initialized_ = false;
    }

private:
    bool initialized_;
    AudioConfig config_;
    std::unique_ptr<AudioDevice> device_;
    std::shared_ptr<AudioProcessor> processor_;
};

// AudioManager implementation
AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

bool AudioManager::initialize() { return impl_->initialize(); }
bool AudioManager::loadConfig(const std::string& configPath) { return impl_->loadConfig(configPath); }
bool AudioManager::saveConfig(const std::string& configPath) { return impl_->saveConfig(configPath); }
std::vector<DeviceInfo> AudioManager::getAvailableDevices() { return impl_->getAvailableDevices(); }
std::shared_ptr<AudioThread> AudioManager::createAudioThread(const AudioConfig& config) { return impl_->createAudioThread(config); }
std::shared_ptr<AudioProcessor> AudioManager::getProcessor() { return impl_->getProcessor(); }
bool AudioManager::updateConfig(const AudioConfig& config) { return impl_->updateConfig(config); }
AudioConfig AudioManager::getCurrentConfig() const { return impl_->getCurrentConfig(); }
void AudioManager::cleanup() { impl_->cleanup(); }

} // namespace audio
} // namespace perfx 