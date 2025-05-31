#include "../../include/core/config_manager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <algorithm>

namespace perfx {
namespace core {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 设置配置文件路径
    std::filesystem::path configDir = std::filesystem::current_path() / "config";
    std::filesystem::create_directories(configDir);
    configFilePath_ = (configDir / "config.json").string();

    // 设置预设目录
    presetsDir_ = (configDir / "presets").string();
    std::filesystem::create_directories(presetsDir_);

    // 初始化默认配置
    initDefaultConfig();

    // 加载配置
    loadConfig();
}

void ConfigManager::initDefaultConfig() {
    // 初始化音频配置
    audioConfig_ = audio::AudioConfig();
    audioConfig_.inputDevice = audio::AudioConfig::getDefaultInputConfig().inputDevice;
    audioConfig_.outputDevice = audio::AudioConfig::getDefaultOutputConfig().outputDevice;
    audioConfig_.recordingPath = (std::filesystem::current_path() / "recordings").string();
    audioConfig_.autoStartRecording = false;
    audioConfig_.maxRecordingDuration = 3600; // 1小时

    // 服务器、用户、设备配置可根据需要自行实现
}

bool ConfigManager::loadConfig() {
    std::ifstream file(configFilePath_);
    if (!file.is_open()) {
        std::cout << "Failed to open config file for reading: " << configFilePath_ << std::endl;
        return false;
    }
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        std::cout << "Config file is empty: " << configFilePath_ << std::endl;
        return false;
    }
    audioConfig_.fromJson(data);
    return true;
}

bool ConfigManager::saveConfig() {
    std::ofstream file(configFilePath_);
    if (!file.is_open()) {
        std::cout << "Failed to open config file for writing: " << configFilePath_ << std::endl;
        return false;
    }
    std::string jsonStr = audioConfig_.toJson();
    file << jsonStr;
    return true;
}

void ConfigManager::setAudioConfig(const audio::AudioConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (validateAudioConfig(config)) {
        audioConfig_ = config;
        saveConfig();
        notifyConfigChanged();
    }
}

// 其余 setServerConfig、updateUserConfig、updateDeviceConfig 可用类似方式实现

void ConfigManager::savePreset(const std::string& name, const audio::AudioConfig& config) {
    if (name.empty()) {
        std::cout << "Cannot save preset with empty name" << std::endl;
        return;
    }
    if (!validateAudioConfig(config)) {
        std::cout << "Cannot save invalid audio config preset" << std::endl;
        return;
    }
    std::filesystem::path presetPath = std::filesystem::path(presetsDir_) / (name + ".json");
    std::ofstream file(presetPath);
    if (!file.is_open()) {
        std::cout << "Failed to open preset file for writing: " << presetPath << std::endl;
        return;
    }
    std::string jsonStr = config.toJson();
    file << jsonStr;
}

audio::AudioConfig ConfigManager::loadPreset(const std::string& name) {
    if (name.empty()) {
        std::cout << "Cannot load preset with empty name" << std::endl;
        return audio::AudioConfig();
    }
    std::filesystem::path presetPath = std::filesystem::path(presetsDir_) / (name + ".json");
    std::ifstream file(presetPath);
    if (!file.is_open()) {
        std::cout << "Preset file does not exist: " << presetPath << std::endl;
        return audio::AudioConfig();
    }
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty()) {
        std::cout << "Preset file is empty: " << presetPath << std::endl;
        return audio::AudioConfig();
    }
    audio::AudioConfig config;
    config.fromJson(data);
    return config;
}

void ConfigManager::deletePreset(const std::string& name) {
    if (name.empty()) {
        std::cout << "Cannot delete preset with empty name" << std::endl;
        return;
    }
    std::filesystem::path presetPath = std::filesystem::path(presetsDir_) / (name + ".json");
    if (!std::filesystem::exists(presetPath)) {
        std::cout << "Preset file does not exist: " << presetPath << std::endl;
        return;
    }
    std::filesystem::remove(presetPath);
}

std::vector<std::string> ConfigManager::getPresetList() const {
    std::vector<std::string> presets;
    for (const auto& entry : std::filesystem::directory_iterator(presetsDir_)) {
        if (entry.path().extension() == ".json") {
            presets.push_back(entry.path().stem().string());
        }
    }
    return presets;
}

void ConfigManager::addConfigChangeListener(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    configChangeListeners_.push_back(callback);
}

void ConfigManager::removeConfigChangeListener(ConfigChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = std::find_if(configChangeListeners_.begin(), 
                          configChangeListeners_.end(), 
                          [&callback](const ConfigChangeCallback& c) { return &c == &callback; });
    if (it != configChangeListeners_.end()) {
        configChangeListeners_.erase(it);
    }
}

bool ConfigManager::validateConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return validateAudioConfig(audioConfig_);
}

bool ConfigManager::validateAudioConfig(const audio::AudioConfig& config) const {
    // 验证输入设备
    if (config.inputDevice.maxInputChannels <= 0 || 
        config.inputDevice.defaultSampleRate <= 0) {
        lastError_ = "Invalid input device configuration";
        return false;
    }
    // 验证输出设备
    if (config.outputDevice.maxOutputChannels <= 0 || 
        config.outputDevice.defaultSampleRate <= 0) {
        lastError_ = "Invalid output device configuration";
        return false;
    }
    // 验证录音配置
    if (config.maxRecordingDuration <= 0) {
        lastError_ = "Invalid recording duration";
        return false;
    }
    return true;
}

void ConfigManager::notifyConfigChanged() {
    for (const auto& callback : configChangeListeners_) {
        try {
            callback();
        } catch (const std::exception& e) {
            std::cout << "Error in config change callback: " << e.what() << std::endl;
        }
    }
}

} // namespace core
} // namespace perfx 