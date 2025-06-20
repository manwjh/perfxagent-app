//
// ASR 用户层调用示例
// 演示：一行代码完成从文件到ASR识别的完整流程
//

#include <asr/asr_client.h>
#include <asr/asr_manager.h>
#include <iostream>
#include <string>

// 简单的回调类，只处理识别结果
class SimpleCallback : public Asr::AsrCallback {
private:
    std::string m_finalResult;

public:
    void onOpen(Asr::AsrClient* client) override {
        (void)client; // 避免未使用参数警告
        std::cout << "✅ 连接已建立" << std::endl;
    }
    
    void onMessage(Asr::AsrClient* client, const std::string& message) override {
        (void)client; // 避免未使用参数警告
        // 简单的文本提取
        if (message.find("\"text\"") != std::string::npos && 
            message.find("\"is_final\":true") != std::string::npos) {
            
            size_t textStart = message.find("\"text\"");
            size_t valueStart = message.find("\"", textStart + 7);
            if (valueStart != std::string::npos) {
                size_t valueEnd = message.find("\"", valueStart + 1);
                if (valueEnd != std::string::npos) {
                    m_finalResult = message.substr(valueStart + 1, valueEnd - valueStart - 1);
                    std::cout << "🎯 识别结果: " << m_finalResult << std::endl;
                }
            }
        }
    }
    
    void onError(Asr::AsrClient* client, const std::string& error) override {
        (void)client; // 避免未使用参数警告
        std::cout << "❌ 错误: " << error << std::endl;
    }
    
    void onClose(Asr::AsrClient* client) override {
        (void)client; // 避免未使用参数警告
        std::cout << "🔌 连接已关闭" << std::endl;
    }
    
    std::string getResult() const { return m_finalResult; }
};

int main() {
    std::cout << "=== ASR 用户层调用示例 ===" << std::endl;
    std::cout << "🎯 目标：一行代码完成文件到ASR识别的完整流程" << std::endl;
    std::cout << "" << std::endl;
    
    // 音频文件路径
    std::string audioFile = "/Users/wangjunhui/playcode/音频测试样本/38s.wav";
    
    // 创建管理器和回调
    Asr::AsrManager manager;
    SimpleCallback callback;
    manager.setCallback(&callback);
    
    // 🎯 核心：一行代码完成完整流程！
    std::cout << "🚀 执行：manager.recognizeAudioFile(audioFile)" << std::endl;
    std::cout << "" << std::endl;
    
    bool success = manager.recognizeAudioFile(audioFile);
    
    // 显示结果
    if (success) {
        std::string result = callback.getResult();
        if (!result.empty()) {
            std::cout << "" << std::endl;
            std::cout << "🎉 识别成功！" << std::endl;
            std::cout << "📝 最终结果: " << result << std::endl;
        } else {
            std::cout << "⚠️  未收到识别结果" << std::endl;
        }
    } else {
        std::cout << "❌ 识别失败" << std::endl;
        return 1;
    }
    
    return 0;
} 