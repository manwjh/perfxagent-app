#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QFile>
#include <QtCore/QObject>
#include <memory>
#include <functional>
#include <mutex>
#include "audio/audio_device_info.h"
#include "audio/audio_config.h"

namespace perfx {
namespace core {

// 服务器配置结构
struct ServerConfig {
    QString url;            // 服务器URL
    QString accessToken;    // 访问令牌
    QString deviceId;       // 设备ID
    QString clientId;       // 客户端ID

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["url"] = url;
        obj["accessToken"] = accessToken;
        obj["deviceId"] = deviceId;
        obj["clientId"] = clientId;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        url = obj["url"].toString();
        accessToken = obj["accessToken"].toString();
        deviceId = obj["deviceId"].toString();
        clientId = obj["clientId"].toString();
    }
};

// 用户配置结构
struct UserConfig {
    QString username;       // 用户名
    QString password;       // 密码（加密存储）
    QString agentName;      // 绑定的Agent名称
    QString agentType;      // Agent类型
    QJsonObject extraInfo;  // 其他从服务器获取的信息

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["username"] = username;
        obj["password"] = password;
        obj["agentName"] = agentName;
        obj["agentType"] = agentType;
        obj["extraInfo"] = extraInfo;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        username = obj["username"].toString();
        password = obj["password"].toString();
        agentName = obj["agentName"].toString();
        agentType = obj["agentType"].toString();
        extraInfo = obj["extraInfo"].toObject();
    }
};

// 设备配置结构
struct DeviceConfig {
    QString deviceName;     // 设备名称
    QString osVersion;      // 操作系统版本
    QString appVersion;     // 应用版本
    QString deviceType;     // 设备类型
    QJsonObject systemInfo; // 系统信息

    QJsonObject toJson() const {
        QJsonObject obj;
        obj["deviceName"] = deviceName;
        obj["osVersion"] = osVersion;
        obj["appVersion"] = appVersion;
        obj["deviceType"] = deviceType;
        obj["systemInfo"] = systemInfo;
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        deviceName = obj["deviceName"].toString();
        osVersion = obj["osVersion"].toString();
        appVersion = obj["appVersion"].toString();
        deviceType = obj["deviceType"].toString();
        systemInfo = obj["systemInfo"].toObject();
    }
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager& getInstance();

    // 音频配置
    void setAudioConfig(const perfx::AudioConfig& config);
    perfx::AudioConfig getAudioConfig() const;

    // 服务器配置
    void setServerConfig(const ServerConfig& config);
    ServerConfig getServerConfig() const;

    // 用户配置
    const UserConfig& getUserConfig() const { return userConfig_; }
    void updateUserConfig(const UserConfig& config);

    // 设备配置
    const DeviceConfig& getDeviceConfig() const { return deviceConfig_; }
    void updateDeviceConfig(const DeviceConfig& config);

    // 配置变更回调函数类型
    using ConfigChangeCallback = std::function<void()>;

    // 加载和保存配置
    bool loadConfig();
    bool saveConfig();

    // 获取配置
    const perfx::AudioConfig& getAudioConfigStruct() const { return audioConfig_; }
    const ServerConfig& getServerConfigStruct() const { return serverConfig_; }
    const UserConfig& getUserConfigStruct() const { return userConfig_; }
    const DeviceConfig& getDeviceConfigStruct() const { return deviceConfig_; }

    // 配置预设管理
    void savePreset(const QString& name, const perfx::AudioConfig& config);
    perfx::AudioConfig loadPreset(const QString& name);
    QStringList getPresetList() const;
    void deletePreset(const QString& name);

    // 配置变更通知
    void addConfigChangeListener(ConfigChangeCallback callback);
    void removeConfigChangeListener(ConfigChangeCallback callback);

    // 配置验证
    bool validateConfig() const;
    QString getLastError() const { return lastError_; }

signals:
    void configChanged();
    void presetChanged(const QString& name);

private:
    ConfigManager();
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    QString configFilePath_;
    QString presetsDir_;
    perfx::AudioConfig audioConfig_;
    ServerConfig serverConfig_;
    UserConfig userConfig_;
    DeviceConfig deviceConfig_;
    mutable QString lastError_;
    std::vector<ConfigChangeCallback> configChangeListeners_;
    mutable std::mutex mutex_;

    void initDefaultConfig();
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);
    void notifyConfigChanged();
    bool validateAudioConfig(const perfx::AudioConfig& config) const;
};

} // namespace core
} // namespace perfx 