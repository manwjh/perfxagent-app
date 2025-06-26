#pragma once

#include <string>
#include <vector>

namespace Asr {

/**
 * @brief 安全的密钥管理器
 * 
 * 使用多层混淆和加密技术保护API密钥
 * 包括：字符串混淆、简单加密、运行时解密
 * 独立于Qt，专门用于ASR模块
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
     * @brief 检查是否为体验模式（从代码空间获取凭证）
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

} // namespace Asr 