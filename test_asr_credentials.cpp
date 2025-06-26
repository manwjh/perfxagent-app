#include <iostream>
#include <string>
#include <cstdlib>
#include "include/ui/config_manager.h"
#include "include/asr/asr_manager.h"

using namespace perfx::ui;
using namespace perfx::asr;

void testCredentialsFlow() {
    std::cout << "=== ASR凭证流程验证 ===" << std::endl;
    std::cout << std::endl;
    
    // 1. 测试环境变量配置
    std::cout << "1. 检查环境变量配置..." << std::endl;
    const char* envAppId = std::getenv("ASR_APP_ID");
    const char* envAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* envSecretKey = std::getenv("ASR_SECRET_KEY");
    
    if (envAppId && envAccessToken) {
        std::cout << "   ✅ 环境变量已设置" << std::endl;
        std::cout << "   - App ID: " << envAppId << std::endl;
        std::cout << "   - Access Token: " << envAccessToken << std::endl;
        std::cout << "   - Secret Key: " << (envSecretKey ? envSecretKey : "未设置") << std::endl;
    } else {
        std::cout << "   ⚠️  环境变量未设置" << std::endl;
    }
    std::cout << std::endl;
    
    // 2. 测试SecureKeyManager（混淆配置）
    std::cout << "2. 测试混淆配置..." << std::endl;
    std::string obfuscatedAppId = SecureKeyManager::getAppId();
    std::string obfuscatedAccessToken = SecureKeyManager::getAccessToken();
    std::string obfuscatedSecretKey = SecureKeyManager::getSecretKey();
    
    std::cout << "   ✅ 混淆配置获取成功" << std::endl;
    std::cout << "   - App ID: " << obfuscatedAppId << std::endl;
    std::cout << "   - Access Token: " << obfuscatedAccessToken << std::endl;
    std::cout << "   - Secret Key: " << obfuscatedSecretKey << std::endl;
    std::cout << std::endl;
    
    // 3. 测试ConfigManager配置加载
    std::cout << "3. 测试ConfigManager配置加载..." << std::endl;
    ConfigManager* configManager = ConfigManager::instance();
    AsrConfig config = configManager->loadConfig();
    
    std::cout << "   ✅ 配置加载成功" << std::endl;
    std::cout << "   - 配置来源: " << config.configSource << std::endl;
    std::cout << "   - App ID: " << config.appId << std::endl;
    std::cout << "   - Access Token: " << config.accessToken << std::endl;
    std::cout << "   - Secret Key: " << config.secretKey << std::endl;
    std::cout << "   - 配置有效: " << (config.isValid ? "是" : "否") << std::endl;
    std::cout << std::endl;
    
    // 4. 测试AsrManager配置加载
    std::cout << "4. 测试AsrManager配置加载..." << std::endl;
    AsrManager asrManager;
    AsrConfig asrConfig = asrManager.getConfig();
    
    std::cout << "   ✅ AsrManager配置获取成功" << std::endl;
    std::cout << "   - 配置来源: " << asrConfig.configSource << std::endl;
    std::cout << "   - App ID: " << asrConfig.appId << std::endl;
    std::cout << "   - Access Token: " << asrConfig.accessToken << std::endl;
    std::cout << "   - Secret Key: " << asrConfig.secretKey << std::endl;
    std::cout << "   - 配置有效: " << (asrConfig.isValid ? "是" : "否") << std::endl;
    std::cout << std::endl;
    
    // 5. 测试体验模式检测
    std::cout << "5. 测试体验模式检测..." << std::endl;
    bool isTrialMode = SecureKeyManager::isTrialMode();
    std::cout << "   - 体验模式: " << (isTrialMode ? "是" : "否") << std::endl;
    if (isTrialMode) {
        std::cout << "   💡 当前使用混淆配置（体验模式）" << std::endl;
    } else {
        std::cout << "   💡 当前使用环境变量配置" << std::endl;
    }
    std::cout << std::endl;
    
    // 6. 验证凭证一致性
    std::cout << "6. 验证凭证一致性..." << std::endl;
    bool configConsistent = (config.appId == asrConfig.appId && 
                           config.accessToken == asrConfig.accessToken &&
                           config.secretKey == asrConfig.secretKey);
    
    if (configConsistent) {
        std::cout << "   ✅ 配置一致性验证通过" << std::endl;
    } else {
        std::cout << "   ❌ 配置一致性验证失败" << std::endl;
        std::cout << "   - ConfigManager App ID: " << config.appId << std::endl;
        std::cout << "   - AsrManager App ID: " << asrConfig.appId << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== 验证完成 ===" << std::endl;
}

int main() {
    try {
        testCredentialsFlow();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
} 