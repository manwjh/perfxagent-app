#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <filesystem>
#include "audio/audio_types.h"

namespace perfx {
namespace core {

class ConfigManager {
public:
    using ConfigChangeCallback = std::function<void()>;

    static ConfigManager& getInstance();

    ConfigManager();
    bool loadConfig();
    bool saveConfig();

    void setAudioConfig(const audio::AudioConfig& config);
    const audio::AudioConfig& getAudioConfig() const { return audioConfig_; }

    void savePreset(const std::string& name, const audio::AudioConfig& config);
    audio::AudioConfig loadPreset(const std::string& name);
    void deletePreset(const std::string& name);
    std::vector<std::string> getPresetList() const;

    void addConfigChangeListener(ConfigChangeCallback callback);
    void removeConfigChangeListener(ConfigChangeCallback callback);

    bool validateConfig() const;
    bool validateAudioConfig(const audio::AudioConfig& config) const;

    std::string getLastError() const { return lastError_; }

private:
    void initDefaultConfig();
    void notifyConfigChanged();

    audio::AudioConfig audioConfig_;
    std::string configFilePath_;
    std::string presetsDir_;
    mutable std::mutex mutex_;
    std::vector<ConfigChangeCallback> configChangeListeners_;
    mutable std::string lastError_;
};

} // namespace core
} // namespace perfx 