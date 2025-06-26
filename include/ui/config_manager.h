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
    std::string configSource;  // "environment_variables", "default_test_config"
    std::string lastModified;
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
     * @brief 加载配置
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
    
    // 默认测试配置（软件发布方提供）
    struct DefaultConfig {
        //std::string appId = "8388344882";
        //std::string accessToken = "vQWuOVrgH6J0kCAoHcQZ_wZfA5q1111";    //无效配置，你需要自己写。
        //std::string secretKey = "oKzfTdLm0M2dVUXUKW86jb-hFLGP1111";
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