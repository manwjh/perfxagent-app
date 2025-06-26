#include "ui/config_manager.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QSettings>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <random>
#include <chrono>

namespace perfx {
namespace ui {

// ============================================================================
// SecureKeyManager 实现
// ============================================================================

// 混淆后的密钥数据（使用XOR加密和字符串混淆）
// 这些数据看起来像随机字节，实际上是经过多层混淆的真实密钥
namespace {
    // 自动更新于 2025-06-25 23:30:28
    // 自动更新于 2025-06-26 16:08:50
    // 混淆后的App ID
    const std::vector<uint8_t> OBFUSCATED_APP_ID = {
        0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70, 0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76,
        0x76, 0x7A, 0x7A, 0x70,
    };
    
    // 混淆后的Access Token
    const std::vector<uint8_t> OBFUSCATED_ACCESS_TOKEN = {
        0x34, 0x13, 0x15, 0x37, 0x0D, 0x14, 0x30, 0x25, 0x0A, 0x74, 0x08, 0x72, 0x29, 0x01, 0x03, 0x13,
        0x2D, 0x0A, 0x21, 0x13, 0x18, 0x1D, 0x35, 0x18, 0x24, 0x03, 0x77, 0x33, 0x70, 0x2E, 0x05, 0x71,
        0x34, 0x13, 0x15, 0x37, 0x0D, 0x14, 0x30, 0x25, 0x0A, 0x74, 0x08, 0x72, 0x29, 0x01, 0x03, 0x13,
        0x2D, 0x0A, 0x21, 0x13, 0x18, 0x1D, 0x35, 0x18, 0x24, 0x03, 0x77, 0x33, 0x70, 0x2E, 0x05, 0x71,
    };
    
    // 混淆后的Secret Key
    const std::vector<uint8_t> OBFUSCATED_SECRET_KEY = {
        0x2D, 0x09, 0x38, 0x24, 0x16, 0x26, 0x0E, 0x2F, 0x72, 0x0F, 0x70, 0x26, 0x14, 0x17, 0x1A, 0x17,
        0x09, 0x15, 0x7A, 0x74, 0x28, 0x20, 0x6F, 0x2A, 0x04, 0x0E, 0x05, 0x12, 0x2F, 0x05, 0x71, 0x27,
        0x2D, 0x09, 0x38, 0x24, 0x16, 0x26, 0x0E, 0x2F, 0x72, 0x0F, 0x70, 0x26, 0x14, 0x17, 0x1A, 0x17,
        0x09, 0x15, 0x7A, 0x74, 0x28, 0x20, 0x6F, 0x2A, 0x04, 0x0E, 0x05, 0x12, 0x2F, 0x05, 0x71, 0x27,
    };
    
    // XOR密钥（用于简单加密）
    const uint8_t XOR_KEY = 0x42;
}

std::string SecureKeyManager::decryptString(const std::vector<uint8_t>& encrypted, uint8_t key) {
    std::string result;
    result.reserve(encrypted.size());
    
    for (uint8_t byte : encrypted) {
        result.push_back(static_cast<char>(byte ^ key));
    }
    
    return result;
}

std::string SecureKeyManager::deobfuscateString(const std::vector<uint8_t>& obfuscated) {
    // 简单的字符串混淆：取前半部分作为真实数据
    size_t halfSize = obfuscated.size() / 2;
    std::vector<uint8_t> realData(obfuscated.begin(), obfuscated.begin() + halfSize);
    
    return decryptString(realData, XOR_KEY);
}

std::vector<uint8_t> SecureKeyManager::getObfuscatedAppId() {
    return OBFUSCATED_APP_ID;
}

std::vector<uint8_t> SecureKeyManager::getObfuscatedAccessToken() {
    return OBFUSCATED_ACCESS_TOKEN;
}

std::vector<uint8_t> SecureKeyManager::getObfuscatedSecretKey() {
    return OBFUSCATED_SECRET_KEY;
}

std::string SecureKeyManager::getAppId() {
    return deobfuscateString(getObfuscatedAppId());
}

std::string SecureKeyManager::getAccessToken() {
    return deobfuscateString(getObfuscatedAccessToken());
}

std::string SecureKeyManager::getSecretKey() {
    return deobfuscateString(getObfuscatedSecretKey());
}

bool SecureKeyManager::isTrialMode() {
    // 检查是否有用户自定义的ASR_环境变量
    const char* userAppId = std::getenv("ASR_APP_ID");
    const char* userAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    
    // 如果没有用户配置，则为体验模式（从代码空间获取凭证）
    return !(userAppId && userAccessToken);
}

// ============================================================================
// ConfigManager 实现
// ============================================================================

// 单例实例
static ConfigManager* s_instance = nullptr;

ConfigManager* ConfigManager::instance() {
    if (!s_instance) {
        s_instance = new ConfigManager();
    }
    return s_instance;
}

ConfigManager::ConfigManager()
    : configLoaded_(false)
{
    // 设置配置文件路径
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) {
        configDir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.perfxagent";
    }
    configFilePath_ = configDir + "/asr_config.json";
    
    // 确保配置目录存在
    ensureConfigDirectory();
}

ConfigManager::~ConfigManager() {
    s_instance = nullptr;
}

AsrConfig ConfigManager::loadConfig() {
    // ============================================================================
    // 配置加载 - 支持多种配置方式
    // ============================================================================
    // 配置优先级：
    // 1. 环境变量 ASR_* (用户自定义，优先级最高)
    // 2. 环境变量 VOLC_* (用户自定义，兼容性支持)
    // 3. 界面配置 (用户通过界面设置)
    // 4. 混淆配置 (厂商提供，体验模式)
    // ============================================================================
    
    qDebug() << "==========================================";
    qDebug() << "[ASR-CRED] 🔍 开始加载ASR配置...";
    qDebug() << "[ASR-CRED] 📋 配置优先级:";
    qDebug() << "   1. 环境变量 ASR_* (用户自定义，优先级最高)";
    qDebug() << "   2. 界面配置 (用户通过界面设置)";
    qDebug() << "   3. 混淆配置 (厂商提供，体验模式)";
    qDebug() << "==========================================";
    
    // 第一优先级：环境变量 ASR_*
    const char* envAppId = std::getenv("ASR_APP_ID");
    const char* envAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* envSecretKey = std::getenv("ASR_SECRET_KEY");
    
    if (envAppId && envAccessToken) {
        qDebug() << "[ASR-CRED] 🎯 使用环境变量配置 (ASR_* 前缀)";
        qDebug() << "   💡 这是用户自定义的环境变量配置，优先级最高";
        
        AsrConfig config;
        config.appId = envAppId;
        config.accessToken = envAccessToken;
        config.secretKey = envSecretKey ? envSecretKey : "";
        config.isValid = true;
        config.configSource = "environment_variables";
        
        qDebug() << "[ASR-CRED] ✅ 环境变量配置加载成功";
        qDebug() << "   - App ID: " << QString::fromStdString(config.appId);
        qDebug() << "   - Access Token: " << QString::fromStdString(config.accessToken);
        qDebug() << "   - Secret Key: " << QString::fromStdString(config.secretKey);
        
        return config;
    }
    
    // 第二优先级：环境变量 VOLC_* (兼容性支持)
    const char* volcAppId = std::getenv("VOLC_APP_ID");
    const char* volcAccessToken = std::getenv("VOLC_ACCESS_TOKEN");
    const char* volcSecretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (volcAppId && volcAccessToken) {
        qDebug() << "[ASR-CRED] 🎯 使用环境变量配置 (VOLC_* 前缀)";
        qDebug() << "   💡 这是兼容性环境变量配置";
        
        AsrConfig config;
        config.appId = volcAppId;
        config.accessToken = volcAccessToken;
        config.secretKey = volcSecretKey ? volcSecretKey : "";
        config.isValid = true;
        config.configSource = "environment_variables";
        
        qDebug() << "[ASR-CRED] ✅ VOLC环境变量配置加载成功";
        qDebug() << "   - App ID: " << QString::fromStdString(config.appId);
        qDebug() << "   - Access Token: " << QString::fromStdString(config.accessToken);
        qDebug() << "   - Secret Key: " << QString::fromStdString(config.secretKey);
        
        return config;
    }
    
    // 第三优先级：界面配置
    QSettings settings;
    QString savedAppId = settings.value("asr/appId").toString();
    QString savedAccessToken = settings.value("asr/accessToken").toString();
    QString savedSecretKey = settings.value("asr/secretKey").toString();
    
    if (!savedAppId.isEmpty() && !savedAccessToken.isEmpty()) {
        qDebug() << "[ASR-CRED] 🎯 使用界面配置";
        qDebug() << "   💡 这是通过系统配置界面保存的配置";
        
        AsrConfig config;
        config.appId = savedAppId.toStdString();
        config.accessToken = savedAccessToken.toStdString();
        config.secretKey = savedSecretKey.toStdString();
        config.isValid = true;
        config.configSource = "user_config";
        
        qDebug() << "[ASR-CRED] ✅ 界面配置加载成功";
        qDebug() << "   - App ID: " << savedAppId;
        qDebug() << "   - Access Token: " << savedAccessToken;
        qDebug() << "   - Secret Key: " << savedSecretKey;
        
        return config;
    }
    
    // 第四优先级：混淆配置（体验模式）
    qDebug() << "[ASR-CRED] 🎯 使用体验模式配置";
    qDebug() << "   💡 这是厂商提供的混淆配置，用于体验功能";
    qDebug() << "   💡 建议设置环境变量以获得完整功能";
    
    AsrConfig config;
    config.appId = SecureKeyManager::getAppId();
    config.accessToken = SecureKeyManager::getAccessToken();
    config.secretKey = SecureKeyManager::getSecretKey();
    config.isValid = true;
    config.configSource = "trial_mode";
    
    qDebug() << "[ASR-CRED] ✅ 体验模式配置加载成功";
    qDebug() << "   - App ID: " << QString::fromStdString(config.appId);
    qDebug() << "   - Access Token: " << QString::fromStdString(config.accessToken);
    qDebug() << "   - Secret Key: " << QString::fromStdString(config.secretKey);
    qDebug() << "[ASR-CRED] ⚠️  体验模式：请确保使用次数未超过限制";
    
    return config;
}

bool ConfigManager::saveConfig(const AsrConfig& config) {
    if (!validateConfig(config)) {
        qWarning() << "配置验证失败";
        return false;
    }
    
    // 确保配置被标记为有效
    AsrConfig validConfig = config;
    validConfig.isValid = true;
    if (validConfig.configSource.empty()) {
        validConfig.configSource = "user_config";
    }
    
    if (saveToFile(validConfig)) {
        currentConfig_ = validConfig;
        configLoaded_ = true;
        emit configUpdated();
        return true;
    }
    
    return false;
}

bool ConfigManager::testConfig(const AsrConfig& config) {
    if (!validateConfig(config)) {
        return false;
    }
    
    // 这里可以添加实际的ASR连接测试
    // 暂时返回true，表示配置格式正确
    return true;
}

QString ConfigManager::getConfigStatus() const {
    QString status;
    
    if (configLoaded_) {
        if (currentConfig_.configSource == "environment_variables") {
            status = "✅ 使用用户环境变量配置";
            status += "\n🔧 配置来源: ASR_* 环境变量";
        } else if (currentConfig_.configSource == "user_config") {
            status = "✅ 使用用户配置";
            status += "\n🔧 配置来源: 系统配置界面";
        } else if (currentConfig_.configSource == "default_test_config") {
            status = "⚠️ 使用默认测试配置";
            status += "\n💡 建议设置环境变量以获得完整功能";
            status += "\n🔧 设置方法: export ASR_APP_ID=your_app_id";
        } else {
            status = "❌ 配置无效";
            status += "\n🔧 请检查配置或联系技术支持";
        }
        
        if (configFileExists()) {
            QFileInfo fileInfo(configFilePath_);
            status += QString("\n📁 配置文件: %1 (修改时间: %2)")
                        .arg(configFilePath_)
                        .arg(fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss"));
        }
    } else {
        status = "🔄 配置未加载";
    }
    
    return status;
}

bool ConfigManager::hasValidConfig() const {
    if (!configLoaded_) {
        return false;
    }
    
    // 检查关键认证信息是否存在
    if (currentConfig_.appId.empty() || currentConfig_.accessToken.empty()) {
        return false;
    }
    
    // 检查配置是否标记为有效
    if (!currentConfig_.isValid) {
        return false;
    }
    
    // 检查配置来源是否合理
    if (currentConfig_.configSource.empty()) {
        return false;
    }
    
    return true;
}

bool ConfigManager::setEnvironmentVariables(const AsrConfig& config) {
    // 注意：在实际生产环境中，用户应该手动设置环境变量
    // 这里仅用于测试目的
    qDebug() << "⚠️ 设置环境变量（仅用于测试）";
    
    // 在Qt中设置环境变量（仅对当前进程有效）
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("ASR_APP_ID", QString::fromStdString(config.appId));
    env.insert("ASR_ACCESS_TOKEN", QString::fromStdString(config.accessToken));
    env.insert("ASR_SECRET_KEY", QString::fromStdString(config.secretKey));
    
    return true;
}

void ConfigManager::clearEnvironmentVariables() {
    qDebug() << "清除环境变量";
    // 在实际应用中，用户需要手动清除环境变量
}

QString ConfigManager::getConfigFilePath() const {
    return configFilePath_;
}

bool ConfigManager::configFileExists() const {
    return QFile::exists(configFilePath_);
}

bool ConfigManager::deleteConfigFile() {
    if (QFile::exists(configFilePath_)) {
        return QFile::remove(configFilePath_);
    }
    return true;
}

AsrConfig ConfigManager::resetToDefault() {
    AsrConfig config;
    
    // 使用SecureKeyManager获取默认的API密钥
    config.appId = SecureKeyManager::getAppId();
    config.accessToken = SecureKeyManager::getAccessToken();
    config.secretKey = SecureKeyManager::getSecretKey();
    config.useVolcPrefix = defaultConfig_.useVolcPrefix;
    config.format = defaultConfig_.format;
    config.sampleRate = defaultConfig_.sampleRate;
    config.bits = defaultConfig_.bits;
    config.channels = defaultConfig_.channels;
    config.language = defaultConfig_.language;
    config.segDuration = defaultConfig_.segDuration;
    config.logLevel = defaultConfig_.logLevel;
    config.enableBusinessLog = defaultConfig_.enableBusinessLog;
    config.enableFlowLog = defaultConfig_.enableFlowLog;
    config.enableDataLog = defaultConfig_.enableDataLog;
    config.enableProtocolLog = defaultConfig_.enableProtocolLog;
    config.enableAudioLog = defaultConfig_.enableAudioLog;
    config.isValid = true;
    config.configSource = "default_test_config";
    
    currentConfig_ = config;
    configLoaded_ = true;
    
    return config;
}

bool ConfigManager::loadFromEnvironment(AsrConfig& config) {
    // 尝试从ASR_前缀的环境变量加载配置
    std::string appId = getEnvVar("ASR_APP_ID");
    std::string accessToken = getEnvVar("ASR_ACCESS_TOKEN");
    std::string secretKey = getEnvVar("ASR_SECRET_KEY");
    
    if (!appId.empty() && !accessToken.empty()) {
        config.appId = appId;
        config.accessToken = accessToken;
        config.secretKey = secretKey;
        config.useVolcPrefix = false;  // 使用ASR_前缀
        config.isValid = true;
        config.configSource = "environment_variables";
        
        // 加载调试配置
        std::string businessLog = getEnvVar("ASR_ENABLE_BUSINESS_LOG");
        std::string flowLog = getEnvVar("ASR_ENABLE_FLOW_LOG");
        std::string dataLog = getEnvVar("ASR_ENABLE_DATA_LOG");
        std::string protocolLog = getEnvVar("ASR_ENABLE_PROTOCOL_LOG");
        std::string audioLog = getEnvVar("ASR_ENABLE_AUDIO_LOG");
        
        config.enableBusinessLog = (businessLog == "1");
        config.enableFlowLog = (flowLog == "1");
        config.enableDataLog = (dataLog == "1");
        config.enableProtocolLog = (protocolLog == "1");
        config.enableAudioLog = (audioLog == "1");
        
        return true;
    }
    
    return false;
}

bool ConfigManager::loadFromFile(AsrConfig& config) {
    QFile file(configFilePath_);
    
    if (!file.exists()) {
        qDebug() << "配置文件不存在:" << configFilePath_;
        return false;
    }
    
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开配置文件:" << configFilePath_;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "配置文件格式错误:" << error.errorString();
        return false;
    }
    
    // 从JSON加载非敏感配置
    QJsonObject json = doc.object();
    
    if (json.contains("audio")) {
        QJsonObject audio = json["audio"].toObject();
        config.format = audio["format"].toString().toStdString();
        config.sampleRate = audio["sampleRate"].toInt();
        config.bits = audio["bits"].toInt();
        config.channels = audio["channels"].toInt();
        config.language = audio["language"].toString().toStdString();
        config.segDuration = audio["segDuration"].toInt();
    }
    
    if (json.contains("debug")) {
        QJsonObject debug = json["debug"].toObject();
        config.logLevel = debug["logLevel"].toString().toStdString();
        config.enableBusinessLog = debug["enableBusinessLog"].toBool();
        config.enableFlowLog = debug["enableFlowLog"].toBool();
        config.enableDataLog = debug["enableDataLog"].toBool();
        config.enableProtocolLog = debug["enableProtocolLog"].toBool();
        config.enableAudioLog = debug["enableAudioLog"].toBool();
    }
    
    if (json.contains("connection")) {
        QJsonObject connection = json["connection"].toObject();
        config.url = connection["url"].toString().toStdString();
        config.handshakeTimeout = connection["handshakeTimeout"].toInt();
        config.connectionTimeout = connection["connectionTimeout"].toInt();
    }
    
    qDebug() << "从文件加载非敏感配置成功:" << configFilePath_;
    return true;
}

bool ConfigManager::saveToFile(const AsrConfig& config) {
    QJsonObject json;
    
    // 保存音频配置（非敏感）
    QJsonObject audio;
    audio["format"] = QString::fromStdString(config.format);
    audio["sampleRate"] = config.sampleRate;
    audio["bits"] = config.bits;
    audio["channels"] = config.channels;
    audio["language"] = QString::fromStdString(config.language);
    audio["segDuration"] = config.segDuration;
    json["audio"] = audio;
    
    // 保存调试配置（非敏感）
    QJsonObject debug;
    debug["logLevel"] = QString::fromStdString(config.logLevel);
    debug["enableBusinessLog"] = config.enableBusinessLog;
    debug["enableFlowLog"] = config.enableFlowLog;
    debug["enableDataLog"] = config.enableDataLog;
    debug["enableProtocolLog"] = config.enableProtocolLog;
    debug["enableAudioLog"] = config.enableAudioLog;
    json["debug"] = debug;
    
    // 保存连接配置（非敏感）
    QJsonObject connection;
    connection["url"] = QString::fromStdString(config.url);
    connection["handshakeTimeout"] = config.handshakeTimeout;
    connection["connectionTimeout"] = config.connectionTimeout;
    json["connection"] = connection;
    
    // 保存元数据
    QJsonObject metadata;
    metadata["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["configSource"] = QString::fromStdString(config.configSource);
    json["metadata"] = metadata;
    
    QJsonDocument doc(json);
    
    if (!ensureConfigDirectory()) {
        qWarning() << "无法创建配置目录";
        return false;
    }
    
    QFile file(configFilePath_);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "无法写入配置文件:" << configFilePath_;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "非敏感配置已保存到文件:" << configFilePath_;
    return true;
}

bool ConfigManager::validateConfig(const AsrConfig& config) {
    if (config.appId.empty()) {
        qWarning() << "应用ID不能为空";
        return false;
    }
    
    if (config.accessToken.empty()) {
        qWarning() << "访问令牌不能为空";
        return false;
    }
    
    if (config.sampleRate < 8000 || config.sampleRate > 48000) {
        qWarning() << "采样率必须在8000-48000之间";
        return false;
    }
    
    if (config.bits != 8 && config.bits != 16 && config.bits != 24 && config.bits != 32) {
        qWarning() << "位深度必须是8、16、24或32";
        return false;
    }
    
    if (config.channels < 1 || config.channels > 2) {
        qWarning() << "声道数必须是1或2";
        return false;
    }
    
    if (config.segDuration < 50 || config.segDuration > 1000) {
        qWarning() << "分段时长必须在50-1000ms之间";
        return false;
    }
    
    return true;
}

bool ConfigManager::ensureConfigDirectory() {
    QDir dir = QFileInfo(configFilePath_).dir();
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

std::string ConfigManager::getEnvVar(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string();
}

QJsonObject ConfigManager::configToJson(const AsrConfig& config) {
    QJsonObject json;
    
    // 只保存非敏感配置
    QJsonObject audio;
    audio["format"] = QString::fromStdString(config.format);
    audio["sampleRate"] = config.sampleRate;
    audio["bits"] = config.bits;
    audio["channels"] = config.channels;
    audio["language"] = QString::fromStdString(config.language);
    audio["segDuration"] = config.segDuration;
    json["audio"] = audio;
    
    QJsonObject debug;
    debug["logLevel"] = QString::fromStdString(config.logLevel);
    debug["enableBusinessLog"] = config.enableBusinessLog;
    debug["enableFlowLog"] = config.enableFlowLog;
    debug["enableDataLog"] = config.enableDataLog;
    debug["enableProtocolLog"] = config.enableProtocolLog;
    debug["enableAudioLog"] = config.enableAudioLog;
    json["debug"] = debug;
    
    QJsonObject connection;
    connection["url"] = QString::fromStdString(config.url);
    connection["handshakeTimeout"] = config.handshakeTimeout;
    connection["connectionTimeout"] = config.connectionTimeout;
    json["connection"] = connection;
    
    QJsonObject metadata;
    metadata["lastModified"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metadata["configSource"] = QString::fromStdString(config.configSource);
    json["metadata"] = metadata;
    
    return json;
}

AsrConfig ConfigManager::jsonToConfig(const QJsonObject& json) {
    AsrConfig config;
    
    // 只加载非敏感配置
    if (json.contains("audio")) {
        QJsonObject audio = json["audio"].toObject();
        config.format = audio["format"].toString().toStdString();
        config.sampleRate = audio["sampleRate"].toInt();
        config.bits = audio["bits"].toInt();
        config.channels = audio["channels"].toInt();
        config.language = audio["language"].toString().toStdString();
        config.segDuration = audio["segDuration"].toInt();
    }
    
    if (json.contains("debug")) {
        QJsonObject debug = json["debug"].toObject();
        config.logLevel = debug["logLevel"].toString().toStdString();
        config.enableBusinessLog = debug["enableBusinessLog"].toBool();
        config.enableFlowLog = debug["enableFlowLog"].toBool();
        config.enableDataLog = debug["enableDataLog"].toBool();
        config.enableProtocolLog = debug["enableProtocolLog"].toBool();
        config.enableAudioLog = debug["enableAudioLog"].toBool();
    }
    
    if (json.contains("connection")) {
        QJsonObject connection = json["connection"].toObject();
        config.url = connection["url"].toString().toStdString();
        config.handshakeTimeout = connection["handshakeTimeout"].toInt();
        config.connectionTimeout = connection["connectionTimeout"].toInt();
    }
    
    return config;
}

} // namespace perfx
} // namespace ui 