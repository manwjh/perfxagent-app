# API密钥配置指南

## 概述

本系统支持多种API密钥配置方式，按照优先级顺序加载，确保用户配置的灵活性和系统的可用性。

## 配置优先级

### 1. 环境变量 ASR_* (用户自定义，优先级最高)
```bash
# 推荐方式：使用ASR_前缀
export ASR_APP_ID=your_app_id
export ASR_ACCESS_TOKEN=your_access_token
export ASR_SECRET_KEY=your_secret_key
```

**特点：**
- 优先级最高，会覆盖其他所有配置
- 安全性最高，密钥不会保存在文件中
- 适合生产环境使用
- 支持动态修改，无需重启应用

### 2. 环境变量 VOLC_* (用户自定义，兼容性支持)
```bash
# 兼容方式：使用VOLC_前缀
export VOLC_APP_ID=your_app_id
export VOLC_ACCESS_TOKEN=your_access_token
export VOLC_SECRET_KEY=your_secret_key
```

**特点：**
- 兼容火山引擎官方文档的命名规范
- 当ASR_前缀不存在时自动使用
- 同样具有高安全性

### 3. 界面配置 (用户通过界面设置)
通过 `src/ui/system_config_window.cpp` 提供的配置界面设置：

```cpp
// 用户可以在界面中：
// 1. 输入API密钥信息
// 2. 选择是否使用VOLC_前缀
// 3. 保存到本地配置文件
```

**特点：**
- 用户友好的图形界面
- 配置保存在本地文件中
- 支持前缀选择（ASR_ 或 VOLC_）
- 适合开发环境使用

### 4. 混淆配置 (厂商提供，体验模式)
```cpp
// 使用SecureKeyManager获取混淆的API密钥
config.appId = SecureKeyManager::getAppId();
config.accessToken = SecureKeyManager::getAccessToken();
config.secretKey = SecureKeyManager::getSecretKey();
```

**特点：**
- 厂商提供的体验配置
- 使用多层混淆保护
- 有使用次数限制
- 无需用户配置，开箱即用

## 代码实现

### AsrManager 配置加载
```cpp
// src/asr/asr_manager.cpp
bool AsrManager::loadConfigFromEnv(AsrConfig& config) {
    // 第一优先级：检查用户自定义的ASR_前缀环境变量
    const char* appId = std::getenv("ASR_APP_ID");
    const char* accessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* secretKey = std::getenv("ASR_SECRET_KEY");
    
    // 第二优先级：如果ASR_前缀不存在，尝试VOLC_前缀
    if (!appId) appId = std::getenv("VOLC_APP_ID");
    if (!accessToken) accessToken = std::getenv("VOLC_ACCESS_TOKEN");
    if (!secretKey) secretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (appId && accessToken) {
        // 使用用户环境变量配置
        config.appId = appId;
        config.accessToken = accessToken;
        config.secretKey = secretKey ? secretKey : "";
        config.configSource = "environment_variables";
    } else {
        // 使用厂商提供的混淆配置（体验模式）
        config.appId = SecureKeyManager::getAppId();
        config.accessToken = SecureKeyManager::getAccessToken();
        config.secretKey = SecureKeyManager::getSecretKey();
        config.configSource = "trial_mode";
    }
}
```

### AsrClient 凭据获取
```cpp
// src/asr/asr_client.cpp
AsrClient::Credentials AsrClient::getCredentialsFromEnv() {
    // 第一优先级：检查用户自定义的VOLC_前缀环境变量
    const char* appId = std::getenv("VOLC_APP_ID");
    const char* accessToken = std::getenv("VOLC_ACCESS_TOKEN");
    const char* secretKey = std::getenv("VOLC_SECRET_KEY");
    
    if (appId && accessToken) {
        // 使用用户环境变量配置
        creds.appId = appId;
        creds.accessToken = accessToken;
        creds.secretKey = secretKey ? secretKey : "";
    } else {
        // 使用厂商提供的混淆配置（体验模式）
        creds.appId = SecureKeyManager::getAppId();
        creds.accessToken = SecureKeyManager::getAccessToken();
        creds.secretKey = SecureKeyManager::getSecretKey();
    }
}
```

### ConfigManager 配置管理
```cpp
// src/ui/config_manager.cpp
AsrConfig ConfigManager::loadConfig() {
    // 第一优先级：环境变量 ASR_*
    // 第二优先级：环境变量 VOLC_*
    // 第三优先级：界面配置文件
    // 第四优先级：混淆配置（体验模式）
    
    // 详细的配置加载逻辑...
}
```

## 混淆配置生成

### 生成工具
使用 `scripts/generate_obfuscated_keys.py` 生成混淆配置：

```bash
python3 scripts/generate_obfuscated_keys.py
```

### 混淆技术
1. **XOR加密**：使用固定密钥对原始数据进行XOR运算
2. **字符串混淆**：在真实数据后添加假数据
3. **字节数组表示**：将混淆后的数据转换为C++字节数组

### 自动更新
脚本会自动更新 `src/ui/config_manager.cpp` 中的混淆数据：

```cpp
// 自动生成的混淆API密钥数据
const std::vector<uint8_t> OBFUSCATED_APP_ID = {
    0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70,
    // ... 混淆后的字节数组
};
```

## 配置界面功能

### 界面组件
- **应用ID输入框**：输入火山引擎应用ID
- **访问令牌输入框**：输入访问令牌（支持密码模式）
- **密钥输入框**：输入密钥（支持密码模式）
- **前缀选择复选框**：选择使用ASR_或VOLC_前缀
- **其他配置项**：音频格式、采样率、日志级别等

### 配置保存
```cpp
// 保存配置到本地文件
config.appId = appIdEdit_->text().toStdString();
config.accessToken = accessTokenEdit_->text().toStdString();
config.secretKey = secretKeyEdit_->text().toStdString();
config.useVolcPrefix = useVolcPrefixCheck_->isChecked();
```

## 安全建议

### 1. 生产环境
- 使用环境变量方式配置API密钥
- 避免在代码中硬编码密钥
- 定期更新API密钥
- 使用ASR_前缀，与火山引擎官方区分

### 2. 开发环境
- 可以使用界面配置方式
- 配置保存在本地，便于开发调试
- 注意不要将配置文件提交到版本控制

### 3. 体验模式
- 仅用于功能体验
- 有使用次数限制
- 不适合生产环境使用

## 故障排除

### 1. 配置不生效
检查配置优先级：
1. 确认环境变量是否正确设置
2. 检查配置文件是否存在且格式正确
3. 查看日志输出，确认使用的配置来源

### 2. 体验模式限制
如果遇到体验模式限制：
1. 申请自己的火山引擎API密钥
2. 使用环境变量或界面配置
3. 联系厂商获取更多体验配额

### 3. 混淆配置问题
如果混淆配置解密失败：
1. 重新运行 `generate_obfuscated_keys.py`
2. 检查混淆数据是否正确更新
3. 确认解密逻辑是否正确

## 总结

本系统通过多层次的配置管理，既保证了用户配置的灵活性，又提供了"开箱即用"的体验模式。用户可以根据自己的需求选择合适的配置方式，系统会自动按照优先级顺序加载配置。 