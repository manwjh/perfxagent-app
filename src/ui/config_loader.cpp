#include "ui/config_manager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDebug>
#include <cstdlib>

namespace perfx {
namespace ui {

// ============================================================================
// ConfigLoader 实现
// ============================================================================

AsrConfig ConfigLoader::loadFromEnvironment() {
    AsrConfig config;
    
    // 检查ASR_前缀的环境变量
    const char* envAppId = std::getenv("ASR_APP_ID");
    const char* envAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* envSecretKey = std::getenv("ASR_SECRET_KEY");
    
    if (envAppId && envAccessToken) {
        config.appId = envAppId;
        config.accessToken = envAccessToken;
        config.secretKey = envSecretKey ? envSecretKey : "";
        config.useVolcPrefix = false;
        config.isValid = true;
        config.configSource = "environment_variables";
        return config;
    }
    
    // 检查VOLC_前缀的环境变量（兼容性）
    const char* volcAppId = std::getenv("VOLC_APP_ID");
    const char* volcAccessToken = std::getenv("VOLC_ACCESS_TOKEN");
    const char* volcSecretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (volcAppId && volcAccessToken) {
        config.appId = volcAppId;
        config.accessToken = volcAccessToken;
        config.secretKey = volcSecretKey ? volcSecretKey : "";
        config.useVolcPrefix = true;
        config.isValid = true;
        config.configSource = "environment_variables";
        return config;
    }
    
    // 没有找到环境变量配置
    return config;
}

AsrConfig ConfigLoader::loadFromUserConfig(const QString& configPath) {
    AsrConfig config;
    
    if (!QFile::exists(configPath)) {
        return config;
    }
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return config;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return config;
    }
    
    QJsonObject json = doc.object();
    
    // 加载音频配置
    if (json.contains("audio")) {
        QJsonObject audio = json["audio"].toObject();
        config.format = audio["format"].toString().toStdString();
        config.sampleRate = audio["sampleRate"].toInt();
        config.bits = audio["bits"].toInt();
        config.channels = audio["channels"].toInt();
        config.language = audio["language"].toString().toStdString();
        config.segDuration = audio["segDuration"].toInt();
    }
    
    // 加载调试配置
    if (json.contains("debug")) {
        QJsonObject debug = json["debug"].toObject();
        config.logLevel = debug["logLevel"].toString().toStdString();
        config.enableBusinessLog = debug["enableBusinessLog"].toBool();
        config.enableFlowLog = debug["enableFlowLog"].toBool();
        config.enableDataLog = debug["enableDataLog"].toBool();
        config.enableProtocolLog = debug["enableProtocolLog"].toBool();
        config.enableAudioLog = debug["enableAudioLog"].toBool();
    }
    
    // 加载连接配置
    if (json.contains("connection")) {
        QJsonObject connection = json["connection"].toObject();
        config.url = connection["url"].toString().toStdString();
        config.handshakeTimeout = connection["handshakeTimeout"].toInt();
        config.connectionTimeout = connection["connectionTimeout"].toInt();
    }
    
    config.isValid = true;
    config.configSource = "user_config";
    return config;
}

AsrConfig ConfigLoader::loadTrialModeConfig() {
    AsrConfig config;
    
    // 使用SecureKeyManager获取体验模式的API密钥
    config.appId = SecureKeyManager::getAppId();
    config.accessToken = SecureKeyManager::getAccessToken();
    config.secretKey = SecureKeyManager::getSecretKey();
    config.useVolcPrefix = false;
    config.isValid = true;
    config.configSource = "trial_mode";
    
    return config;
}

std::pair<AsrConfig, ConfigSource> ConfigLoader::loadConfigWithPriority(const QString& configPath) {
    // 第一优先级：环境变量
    if (hasEnvironmentVariables()) {
        AsrConfig config = loadFromEnvironment();
        if (config.isValid) {
            return {config, ConfigSource::ENVIRONMENT_VARIABLES};
        }
    }
    
    // 第二优先级：用户配置文件
    if (hasValidUserConfig(configPath)) {
        AsrConfig config = loadFromUserConfig(configPath);
        if (config.isValid) {
            return {config, ConfigSource::USER_CONFIG};
        }
    }
    
    // 第三优先级：体验模式
    AsrConfig config = loadTrialModeConfig();
    return {config, ConfigSource::TRIAL_MODE};
}

bool ConfigLoader::hasEnvironmentVariables() {
    const char* appId = std::getenv("ASR_APP_ID");
    const char* accessToken = std::getenv("ASR_ACCESS_TOKEN");
    
    if (appId && accessToken) {
        return true;
    }
    
    const char* volcAppId = std::getenv("VOLC_APP_ID");
    const char* volcAccessToken = std::getenv("VOLC_ACCESS_TOKEN");
    
    return (volcAppId && volcAccessToken);
}

bool ConfigLoader::hasValidUserConfig(const QString& configPath) {
    if (!QFile::exists(configPath)) {
        return false;
    }
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    return doc.isObject();
}

} // namespace perfx
} // namespace ui 