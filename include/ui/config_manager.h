#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QMessageBox>
#include <cstdlib>
#include <string>
#include <vector>

namespace perfx {
namespace ui {

/**
 * @brief ASR配置结构体
 */
struct AsrConfig {
    // 认证配置
    std::string appId;
    std::string accessToken;
    std::string secretKey;
    bool useVolcPrefix = false;
    
    // 音频配置
    std::string format = "wav";
    int sampleRate = 16000;
    int bits = 16;
    int channels = 1;
    std::string language = "zh-CN";
    int segDuration = 100;
    
    // 调试配置
    std::string logLevel = "INFO";
    bool enableBusinessLog = false;
    bool enableFlowLog = false;
    bool enableDataLog = false;
    bool enableProtocolLog = false;
    bool enableAudioLog = false;
    
    // 连接配置
    std::string url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
    int handshakeTimeout = 30;
    int connectionTimeout = 30;
    
    // 状态
    bool isValid = false;
    std::string configSource;  // "environment_variables", "user_config", "trial_mode"
    std::string lastModified;
};

/**
 * @brief 配置来源枚举
 */
enum class ConfigSource {
    ENVIRONMENT_VARIABLES,  // 环境变量
    USER_CONFIG,           // 用户配置文件
    TRIAL_MODE,            // 体验模式
    INVALID               // 无效配置
};

/**
 * @brief 安全的密钥管理器
 * 
 * 使用多层混淆和加密技术保护API密钥
 * 包括：字符串混淆、简单加密、运行时解密
 */
class SecureKeyManager {
public:
    /**
     * @brief 获取解密后的App ID
     */
    static std::string getAppId();
    
    /**
     * @brief 获取解密后的Access Token
     */
    static std::string getAccessToken();
    
    /**
     * @brief 获取解密后的Secret Key
     */
    static std::string getSecretKey();
    
    /**
     * @brief 检查是否为体验模式
     */
    static bool isTrialMode();

private:
    /**
     * @brief 简单的XOR解密
     */
    static std::string decryptString(const std::vector<uint8_t>& encrypted, uint8_t key);
    
    /**
     * @brief 字符串混淆解密
     */
    static std::string deobfuscateString(const std::vector<uint8_t>& obfuscated);
    
    /**
     * @brief 获取混淆后的App ID数据
     */
    static std::vector<uint8_t> getObfuscatedAppId();
    
    /**
     * @brief 获取混淆后的Access Token数据
     */
    static std::vector<uint8_t> getObfuscatedAccessToken();
    
    /**
     * @brief 获取混淆后的Secret Key数据
     */
    static std::vector<uint8_t> getObfuscatedSecretKey();
};

/**
 * @brief 配置加载器 - 专门负责配置的加载逻辑
 */
class ConfigLoader {
public:
    /**
     * @brief 从环境变量加载ASR凭证
     * @return 配置对象，如果失败则返回空配置
     */
    static AsrConfig loadFromEnvironment();
    
    /**
     * @brief 从用户配置文件加载配置
     * @param configPath 配置文件路径
     * @return 配置对象，如果失败则返回空配置
     */
    static AsrConfig loadFromUserConfig(const QString& configPath);
    
    /**
     * @brief 加载体验模式配置
     * @return 体验模式配置对象
     */
    static AsrConfig loadTrialModeConfig();
    
    /**
     * @brief 按优先级加载配置
     * @param configPath 用户配置文件路径
     * @return 配置对象和来源信息
     */
    static std::pair<AsrConfig, ConfigSource> loadConfigWithPriority(const QString& configPath);

private:
    /**
     * @brief 检查环境变量是否存在
     */
    static bool hasEnvironmentVariables();
    
    /**
     * @brief 检查用户配置文件是否存在且有效
     */
    static bool hasValidUserConfig(const QString& configPath);
};

/**
 * @brief 配置验证器 - 专门负责配置验证
 */
class ConfigValidator {
public:
    /**
     * @brief 验证ASR配置的完整性
     * @param config 配置对象
     * @return 是否有效
     */
    static bool validateAsrConfig(const AsrConfig& config);
    
    /**
     * @brief 验证音频配置参数
     * @param config 配置对象
     * @return 是否有效
     */
    static bool validateAudioConfig(const AsrConfig& config);
    
    /**
     * @brief 获取验证错误信息
     * @return 错误信息列表
     */
    static QStringList getValidationErrors();

private:
    static QStringList validationErrors_;
};

/**
 * @brief 配置管理器
 * 
 * 负责管理ASR配置的加载、保存和验证
 * 支持环境变量和默认配置的优先级管理
 */
class ConfigManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ConfigManager* instance();
    
    /**
     * @brief 加载配置（重构后的简化版本）
     * @return 配置对象
     */
    AsrConfig loadConfig();
    
    /**
     * @brief 保存配置到本地文件（仅保存非敏感配置）
     * @param config 配置对象
     * @return 是否保存成功
     */
    bool saveConfig(const AsrConfig& config);
    
    /**
     * @brief 测试配置有效性
     * @param config 配置对象
     * @return 是否有效
     */
    bool testConfig(const AsrConfig& config);
    
    /**
     * @brief 获取配置状态信息
     * @return 状态信息字符串
     */
    QString getConfigStatus() const;
    
    /**
     * @brief 检查是否有有效配置
     * @return 是否有效
     */
    bool hasValidConfig() const;
    
    /**
     * @brief 设置环境变量（仅用于测试，实际生产环境用户手动设置）
     * @param config 配置对象
     * @return 是否设置成功
     */
    bool setEnvironmentVariables(const AsrConfig& config);
    
    /**
     * @brief 清除环境变量
     */
    void clearEnvironmentVariables();
    
    /**
     * @brief 获取配置文件路径
     * @return 配置文件路径
     */
    QString getConfigFilePath() const;
    
    /**
     * @brief 检查配置文件是否存在
     * @return 是否存在
     */
    bool configFileExists() const;
    
    /**
     * @brief 删除配置文件
     * @return 是否删除成功
     */
    bool deleteConfigFile();
    
    /**
     * @brief 重置为默认配置
     * @return 默认配置对象
     */
    AsrConfig resetToDefault();

signals:
    /**
     * @brief 配置更新信号
     */
    void configUpdated();

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    ConfigManager();
    
    /**
     * @brief 析构函数
     */
    ~ConfigManager();
    
    /**
     * @brief 从环境变量加载配置
     * @param config 配置对象引用
     * @return 是否加载成功
     */
    bool loadFromEnvironment(AsrConfig& config);
    
    /**
     * @brief 从文件加载配置（仅非敏感配置）
     * @param config 配置对象引用
     * @return 是否加载成功
     */
    bool loadFromFile(AsrConfig& config);
    
    /**
     * @brief 保存配置到文件（仅非敏感配置）
     * @param config 配置对象
     * @return 是否保存成功
     */
    bool saveToFile(const AsrConfig& config);
    
    /**
     * @brief 验证配置
     * @param config 配置对象
     * @return 是否有效
     */
    bool validateConfig(const AsrConfig& config);
    
    /**
     * @brief 确保配置目录存在
     * @return 是否成功
     */
    bool ensureConfigDirectory();
    
    /**
     * @brief 获取环境变量值
     * @param name 环境变量名
     * @return 环境变量值
     */
    std::string getEnvVar(const std::string& name);
    
    /**
     * @brief 将配置转换为JSON对象（仅非敏感配置）
     * @param config 配置对象
     * @return JSON对象
     */
    QJsonObject configToJson(const AsrConfig& config);
    
    /**
     * @brief 从JSON对象创建配置（仅非敏感配置）
     * @param json JSON对象
     * @return 配置对象
     */
    AsrConfig jsonToConfig(const QJsonObject& json);

    // 成员变量
    QString configFilePath_;
    AsrConfig currentConfig_;
    bool configLoaded_;
    ConfigSource currentConfigSource_;
    
    // 默认测试配置（软件发布方提供）
    struct DefaultConfig {
        std::string format = "wav";
        int sampleRate = 16000;
        int bits = 16;
        int channels = 1;
        std::string language = "zh-CN";
        int segDuration = 100;
        std::string logLevel = "INFO";
        bool enableBusinessLog = false;
        bool enableFlowLog = false;
        bool enableDataLog = false;
        bool enableProtocolLog = false;
        bool enableAudioLog = false;
        bool useVolcPrefix = false;
    };
    
    DefaultConfig defaultConfig_;
};

} // namespace perfx
} // namespace ui 