#include "core/config_manager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QJsonArray>

namespace perfx {
namespace core {

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 设置配置文件路径
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    configFilePath_ = configDir + "/config.json";
    
    // 设置预设目录
    presetsDir_ = configDir + "/presets";
    QDir().mkpath(presetsDir_);
    
    // 初始化默认配置
    initDefaultConfig();
    
    // 加载配置
    loadConfig();
}

void ConfigManager::initDefaultConfig() {
    // 初始化音频配置
    audioConfig_.inputDevice = AudioConfig::getDefaultInputConfig();
    audioConfig_.outputDevice = AudioConfig::getDefaultOutputConfig();
    audioConfig_.recordingPath = QStandardPaths::writableLocation(QStandardPaths::MusicLocation) + "/PerfxAgent";
    audioConfig_.autoStartRecording = false;
    audioConfig_.maxRecordingDuration = 3600; // 1小时

    // 初始化服务器配置
    serverConfig_.url = "wss://api.perfxagent.com/ws";
    serverConfig_.accessToken = "";
    serverConfig_.deviceId = "";
    serverConfig_.clientId = "";

    // 初始化用户配置
    userConfig_.username = "";
    userConfig_.password = "";
    userConfig_.agentName = "";
    userConfig_.agentType = "";
    userConfig_.extraInfo = QJsonObject();

    // 初始化设备配置
    deviceConfig_.deviceName = QSysInfo::prettyProductName();
    deviceConfig_.osVersion = QSysInfo::productVersion();
    deviceConfig_.appVersion = "1.0.0";
    deviceConfig_.deviceType = QSysInfo::productType();
    deviceConfig_.systemInfo = QJsonObject();
}

bool ConfigManager::loadConfig() {
    QFile file(configFilePath_);
    if (!file.exists()) {
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open config file for reading:" << file.errorString();
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Failed to parse config file JSON";
        return false;
    }

    fromJson(doc.object());
    return true;
}

bool ConfigManager::saveConfig() {
    QFile file(configFilePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << file.errorString();
        return false;
    }

    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    return true;
}

void ConfigManager::setAudioConfig(const AudioConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (validateAudioConfig(config)) {
        audioConfig_ = config;
        saveConfig();
        notifyConfigChanged();
    }
}

void ConfigManager::setServerConfig(const ServerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    serverConfig_ = config;
    saveConfig();
    notifyConfigChanged();
}

void ConfigManager::updateUserConfig(const UserConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    userConfig_ = config;
    saveConfig();
    notifyConfigChanged();
}

void ConfigManager::updateDeviceConfig(const DeviceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    deviceConfig_ = config;
    saveConfig();
    notifyConfigChanged();
}

void ConfigManager::savePreset(const QString& name, const AudioConfig& config) {
    if (name.isEmpty()) {
        qWarning() << "Cannot save preset with empty name";
        return;
    }

    if (!validateAudioConfig(config)) {
        qWarning() << "Cannot save invalid audio config preset";
        return;
    }

    QString presetPath = presetsDir_ + "/" + name + ".json";
    QFile file(presetPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open preset file for writing:" << file.errorString();
        return;
    }

    QJsonDocument doc(config.toJson());
    if (!file.write(doc.toJson())) {
        qWarning() << "Failed to write preset file:" << file.errorString();
        return;
    }

    emit presetChanged(name);
}

AudioConfig ConfigManager::loadPreset(const QString& name) {
    if (name.isEmpty()) {
        qWarning() << "Cannot load preset with empty name";
        return AudioConfig();
    }

    QString presetPath = presetsDir_ + "/" + name + ".json";
    QFile file(presetPath);
    if (!file.exists()) {
        qWarning() << "Preset file does not exist:" << presetPath;
        return AudioConfig();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open preset file for reading:" << file.errorString();
        return AudioConfig();
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        qWarning() << "Failed to parse preset file JSON";
        return AudioConfig();
    }

    AudioConfig config;
    config.fromJson(doc.object());
    return config;
}

void ConfigManager::deletePreset(const QString& name) {
    if (name.isEmpty()) {
        qWarning() << "Cannot delete preset with empty name";
        return;
    }

    QString presetPath = presetsDir_ + "/" + name + ".json";
    QFile file(presetPath);
    if (!file.exists()) {
        qWarning() << "Preset file does not exist:" << presetPath;
        return;
    }

    if (!file.remove()) {
        qWarning() << "Failed to delete preset file:" << file.errorString();
        return;
    }

    emit presetChanged(name);
}

QStringList ConfigManager::getPresetList() const {
    QDir dir(presetsDir_);
    QStringList presets;
    for (const QFileInfo& file : dir.entryInfoList(QStringList() << "*.json", QDir::Files)) {
        presets << file.baseName();
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

bool ConfigManager::validateAudioConfig(const AudioConfig& config) const {
    // 验证输入设备
    if (config.inputDevice.maxChannels <= 0 || 
        config.inputDevice.defaultSampleRate <= 0 ||
        config.inputDevice.supportedSampleRates.empty()) {
        lastError_ = "Invalid input device configuration";
        return false;
    }

    // 验证输出设备
    if (config.outputDevice.maxChannels <= 0 || 
        config.outputDevice.defaultSampleRate <= 0 ||
        config.outputDevice.supportedSampleRates.empty()) {
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
            qWarning() << "Error in config change callback:" << e.what();
        }
    }
}

QJsonObject ConfigManager::toJson() const {
    QJsonObject obj;
    obj["audio"] = audioConfig_.toJson();
    obj["server"] = serverConfig_.toJson();
    obj["user"] = userConfig_.toJson();
    obj["device"] = deviceConfig_.toJson();
    return obj;
}

void ConfigManager::fromJson(const QJsonObject& obj) {
    if (obj.contains("audio")) {
        audioConfig_.fromJson(obj["audio"].toObject());
    }
    if (obj.contains("server")) {
        serverConfig_.fromJson(obj["server"].toObject());
    }
    if (obj.contains("user")) {
        userConfig_.fromJson(obj["user"].toObject());
    }
    if (obj.contains("device")) {
        deviceConfig_.fromJson(obj["device"].toObject());
    }
}

} // namespace core
} // namespace perfx 