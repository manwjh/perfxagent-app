#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

// 模拟SecureKeyManager的混淆和解密功能
class TestSecureKeyManager {
private:
    // XOR密钥（用于简单加密）
    static const uint8_t XOR_KEY = 0x42;

public:
    static std::string decryptString(const std::vector<uint8_t>& encrypted, uint8_t key) {
        std::string result;
        result.reserve(encrypted.size());
        
        for (uint8_t byte : encrypted) {
            result.push_back(static_cast<char>(byte ^ key));
        }
        
        return result;
    }

    static std::string deobfuscateString(const std::vector<uint8_t>& obfuscated) {
        // 简单的字符串混淆：取前半部分作为真实数据
        size_t halfSize = obfuscated.size() / 2;
        std::vector<uint8_t> realData(obfuscated.begin(), obfuscated.begin() + halfSize);
        
        return decryptString(realData, XOR_KEY);
    }

    static std::string getAppId() {
        // 混淆后的App ID数据
        static const std::vector<uint8_t> obfuscated = {0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70, 0x7A, 0x71, 0x7A, 0x7A, 0x71, 0x76, 0x76, 0x7A, 0x7A, 0x70};
        return deobfuscateString(obfuscated);
    }

    static std::string getAccessToken() {
        // 混淆后的Access Token数据
        static const std::vector<uint8_t> obfuscated = {0x34, 0x13, 0x15, 0x37, 0x0D, 0x14, 0x30, 0x25, 0x0A, 0x74, 0x08, 0x72, 0x29, 0x01, 0x03, 0x13, 0x2D, 0x0A, 0x21, 0x13, 0x18, 0x1D, 0x35, 0x18, 0x24, 0x03, 0x77, 0x33, 0x70, 0x2E, 0x05, 0x71, 0x34, 0x13, 0x15, 0x37, 0x0D, 0x14, 0x30, 0x25, 0x0A, 0x74, 0x08, 0x72, 0x29, 0x01, 0x03, 0x13, 0x2D, 0x0A, 0x21, 0x13, 0x18, 0x1D, 0x35, 0x18, 0x24, 0x03, 0x77, 0x33, 0x70, 0x2E, 0x05, 0x71};
        return deobfuscateString(obfuscated);
    }

    static std::string getSecretKey() {
        // 混淆后的Secret Key数据
        static const std::vector<uint8_t> obfuscated = {0x2D, 0x09, 0x38, 0x24, 0x16, 0x26, 0x0E, 0x2F, 0x72, 0x0F, 0x70, 0x26, 0x14, 0x17, 0x1A, 0x17, 0x09, 0x15, 0x7A, 0x74, 0x28, 0x20, 0x6F, 0x2A, 0x04, 0x0E, 0x05, 0x12, 0x2F, 0x05, 0x71, 0x27, 0x2D, 0x09, 0x38, 0x24, 0x16, 0x26, 0x0E, 0x2F, 0x72, 0x0F, 0x70, 0x26, 0x14, 0x17, 0x1A, 0x17, 0x09, 0x15, 0x7A, 0x74, 0x28, 0x20, 0x6F, 0x2A, 0x04, 0x0E, 0x05, 0x12, 0x2F, 0x05, 0x71, 0x27};
        return deobfuscateString(obfuscated);
    }
};

void testObfuscatedKeys() {
    std::cout << "=== 混淆密钥解密测试 ===" << std::endl;
    std::cout << std::endl;
    
    // 测试解密功能
    std::cout << "1. 测试混淆密钥解密..." << std::endl;
    
    std::string appId = TestSecureKeyManager::getAppId();
    std::string accessToken = TestSecureKeyManager::getAccessToken();
    std::string secretKey = TestSecureKeyManager::getSecretKey();
    
    std::cout << "   ✅ 解密成功" << std::endl;
    std::cout << "   - App ID: " << appId << std::endl;
    std::cout << "   - Access Token: " << accessToken << std::endl;
    std::cout << "   - Secret Key: " << secretKey << std::endl;
    std::cout << std::endl;
    
    // 验证解密结果
    std::cout << "2. 验证解密结果..." << std::endl;
    bool isValid = !appId.empty() && !accessToken.empty() && !secretKey.empty();
    
    if (isValid) {
        std::cout << "   ✅ 解密结果有效" << std::endl;
        std::cout << "   - App ID长度: " << appId.length() << std::endl;
        std::cout << "   - Access Token长度: " << accessToken.length() << std::endl;
        std::cout << "   - Secret Key长度: " << secretKey.length() << std::endl;
    } else {
        std::cout << "   ❌ 解密结果无效" << std::endl;
    }
    std::cout << std::endl;
    
    // 与环境变量对比
    std::cout << "3. 与环境变量对比..." << std::endl;
    const char* envAppId = std::getenv("ASR_APP_ID");
    const char* envAccessToken = std::getenv("ASR_ACCESS_TOKEN");
    const char* envSecretKey = std::getenv("ASR_SECRET_KEY");
    
    if (envAppId && envAccessToken) {
        bool matchesEnv = (appId == envAppId && accessToken == envAccessToken);
        if (matchesEnv) {
            std::cout << "   ✅ 混淆密钥与环境变量匹配" << std::endl;
        } else {
            std::cout << "   ⚠️  混淆密钥与环境变量不匹配" << std::endl;
            std::cout << "   - 混淆App ID: " << appId << std::endl;
            std::cout << "   - 环境App ID: " << envAppId << std::endl;
        }
    } else {
        std::cout << "   ℹ️  环境变量未设置，无法对比" << std::endl;
    }
    std::cout << std::endl;
    
    std::cout << "=== 测试完成 ===" << std::endl;
}

int main() {
    try {
        testObfuscatedKeys();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
} 