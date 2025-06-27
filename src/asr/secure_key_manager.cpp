#include "asr/secure_key_manager.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <QString>

namespace Asr {

// 敏感信息隐码处理函数 (std::string版本)
std::string maskSensitiveInfo(const std::string& input, int visibleStart, int visibleEnd) {
    if (input.length() <= static_cast<size_t>(visibleStart + visibleEnd)) {
        // 如果字符串太短，只显示首尾各1个字符
        if (input.length() <= 2) {
            return input;
        }
        return input.substr(0, 1) + std::string(input.length() - 2, '*') + input.substr(input.length() - 1);
    }
    
    return input.substr(0, visibleStart) + std::string(input.length() - visibleStart - visibleEnd, '*') + input.substr(input.length() - visibleEnd);
}

// ============================================================================
// SecureKeyManager 实现
// ============================================================================

// 混淆后的密钥数据（使用XOR加密和字符串混淆）
// 这些数据看起来像随机字节，实际上是经过多层混淆的真实密钥
namespace {
    // 自动更新于 2025-06-25 23:30:28
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
    // 检查是否有用户自定义的环境变量
    const char* userAppId = std::getenv("ASR_APP_ID");
    if (userAppId) {
        std::cout << "🔐 [SecureKeyManager] 使用环境变量中的App ID" << std::endl;
        std::cout << "   - 来源: ASR_APP_ID 环境变量" << std::endl;
        std::cout << "   - App ID: " << userAppId << std::endl;
        return userAppId;
    }
    
    // 使用混淆配置
    std::string obfuscatedAppId = deobfuscateString(getObfuscatedAppId());
    std::cout << "🔐 [SecureKeyManager] 使用混淆配置中的App ID" << std::endl;
    std::cout << "   - 来源: 厂商混淆配置 (体验模式)" << std::endl;
    std::cout << "   - App ID: " << obfuscatedAppId << std::endl;
    std::cout << "   - 生成工具: scripts/generate_obfuscated_keys.py" << std::endl;
    return obfuscatedAppId;
}

std::string SecureKeyManager::getAccessToken() {
    // 检查是否有用户自定义的环境变量
    const char* userAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    if (userAccessToken) {
        std::cout << "🔐 [SecureKeyManager] 使用环境变量中的Access Token" << std::endl;
        std::cout << "   - 来源: ASR_ACCESS_TOKEN 环境变量" << std::endl;
        std::cout << "   - Access Token: " << maskSensitiveInfo(std::string(userAccessToken)) << std::endl;
        return userAccessToken;
    }
    
    // 使用混淆配置
    std::string obfuscatedAccessToken = deobfuscateString(getObfuscatedAccessToken());
    std::cout << "🔐 [SecureKeyManager] 使用混淆配置中的Access Token" << std::endl;
    std::cout << "   - 来源: 厂商配置 (体验模式)" << std::endl;
    std::cout << "   - Access Token: " << maskSensitiveInfo(obfuscatedAccessToken) << std::endl;
    std::cout << "   - 生成工具: scripts/generate_obfuscated_keys.py" << std::endl;
    return obfuscatedAccessToken;
}

std::string SecureKeyManager::getSecretKey() {
    // 检查是否有用户自定义的环境变量
    const char* userSecretKey = std::getenv("ASR_SECRET_KEY");
    if (userSecretKey) {
        std::cout << "🔐 [SecureKeyManager] 使用环境变量中的Secret Key" << std::endl;
        std::cout << "   - 来源: ASR_SECRET_KEY 环境变量" << std::endl;
        std::cout << "   - Secret Key: " << maskSensitiveInfo(std::string(userSecretKey)) << std::endl;
        return userSecretKey;
    }
    
    // 使用混淆配置
    std::string obfuscatedSecretKey = deobfuscateString(getObfuscatedSecretKey());
    std::cout << "🔐 [SecureKeyManager] 使用混淆配置中的Secret Key" << std::endl;
    std::cout << "   - 来源: 厂商混淆配置 (体验模式)" << std::endl;
    std::cout << "   - Secret Key: " << maskSensitiveInfo(obfuscatedSecretKey) << std::endl;
    std::cout << "   - 生成工具: scripts/generate_obfuscated_keys.py" << std::endl;
    return obfuscatedSecretKey;
}

bool SecureKeyManager::isTrialMode() {
    // 检查是否有用户自定义的ASR_环境变量
    const char* userAppId = std::getenv("ASR_APP_ID");
    const char* userAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    
    bool isTrial = !(userAppId && userAccessToken);
    
    std::cout << "🔍 [SecureKeyManager] 模式检测结果:" << std::endl;
    if (isTrial) {
        std::cout << "   - 当前模式: 体验模式 (Trial Mode)" << std::endl;
        std::cout << "   - 使用厂商混淆配置" << std::endl;
        std::cout << "   - 建议: 设置环境变量以获得更好的控制" << std::endl;
    } else {
        std::cout << "   - 当前模式: 用户配置模式 (User Config Mode)" << std::endl;
        std::cout << "   - 使用环境变量配置" << std::endl;
        std::cout << "   - App ID: " << userAppId << std::endl;
        std::cout << "   - Access Token: " << maskSensitiveInfo(std::string(userAccessToken)) << std::endl;
    }
    
    // 如果没有用户配置，则为体验模式（从代码空间获取凭证）
    return isTrial;
}

} // namespace Asr 