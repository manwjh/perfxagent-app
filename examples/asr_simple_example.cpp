//
// ASR 用户层调用示例
// 演示：一行代码完成从文件到ASR识别的完整流程
//

#include <asr/asr_client.h>
#include <asr/asr_manager.h>
#include <iostream>
#include <string>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

// 增强的回调类，处理识别结果和状态信息
class EnhancedCallback : public Asr::AsrCallback {
private:
    std::string m_finalResult;
    std::vector<std::string> m_allResults;
    bool m_hasFinalResult = false;
    std::string m_logId;
    std::map<std::string, std::string> m_metadata;

public:
    void onOpen(Asr::AsrClient* client) override {
        (void)client; // 避免未使用参数警告
        std::cout << "✅ 连接已建立" << std::endl;
    }
    
    void onMessage(Asr::AsrClient* client, const std::string& message) override {
        (void)client; // 避免未使用参数警告
        
        try {
            // 尝试解析JSON
            json response = json::parse(message);
            
            // 提取日志ID
            if (response.contains("log_id")) {
                m_logId = response["log_id"];
            }
            
            // 检查是否为最终结果
            if (response.contains("result") && response["result"].contains("is_final")) {
                bool isFinal = response["result"]["is_final"];
                
                if (response["result"].contains("text")) {
                    std::string text = response["result"]["text"];
                    m_allResults.push_back(text);
                    
                    if (isFinal) {
                        m_finalResult = text;
                        m_hasFinalResult = true;
                        
                        // 输出格式化的JSON结果
                        std::cout << "\n🎯 最终识别结果:" << std::endl;
                        std::cout << "┌─────────────────────────────────────────┐" << std::endl;
                        std::cout << "│ 文本: " << text << std::endl;
                        std::cout << "│ 日志ID: " << m_logId << std::endl;
                        std::cout << "│ 置信度: " << (response["result"].contains("confidence") ? 
                                    std::to_string(response["result"]["confidence"].get<double>()) : "N/A") << std::endl;
                        std::cout << "└─────────────────────────────────────────┘" << std::endl;
                    } else {
                        // 中间结果
                        std::cout << "🔄 中间结果: " << text << std::endl;
                    }
                }
            }
            
            // 提取元数据
            if (response.contains("metadata")) {
                for (auto& [key, value] : response["metadata"].items()) {
                    m_metadata[key] = value.dump();
                }
            }
            
        } catch (const json::exception& e) {
            // 如果不是JSON格式，尝试简单的文本提取（兼容旧格式）
            if (message.find("\"text\"") != std::string::npos && 
                message.find("\"is_final\":true") != std::string::npos) {
                
                size_t textStart = message.find("\"text\"");
                size_t valueStart = message.find("\"", textStart + 7);
                if (valueStart != std::string::npos) {
                    size_t valueEnd = message.find("\"", valueStart + 1);
                    if (valueEnd != std::string::npos) {
                        m_finalResult = message.substr(valueStart + 1, valueEnd - valueStart - 1);
                        m_hasFinalResult = true;
                        std::cout << "🎯 识别结果: " << m_finalResult << std::endl;
                    }
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
    bool hasFinalResult() const { return m_hasFinalResult; }
    std::vector<std::string> getAllResults() const { return m_allResults; }
    std::string getLogId() const { return m_logId; }
    std::map<std::string, std::string> getMetadata() const { return m_metadata; }
};

void printUsage(const char* programName) {
    std::cout << "用法: " << programName << " [音频文件路径]" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "示例:" << std::endl;
    std::cout << "  " << programName << " audio.wav" << std::endl;
    std::cout << "  " << programName << "                    # 使用默认音频文件" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "说明:" << std::endl;
    std::cout << "  这是一个简单的ASR调用示例，演示如何一行代码完成音频识别" << std::endl;
    std::cout << "  如需调试ASR模块，请设置环境变量 ASR_DEBUG=1" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string audioFile;
    
    // 解析命令行参数
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else {
            audioFile = arg;
        }
    }
    
    // 检查音频文件参数
    if (audioFile.empty()) {
        // 使用默认音频文件路径
        audioFile = "/Users/wangjunhui/playcode/音频测试样本/38s.wav";
        std::cout << "⚠️  未指定音频文件，使用默认路径: " << audioFile << std::endl;
    }
    
    std::cout << "=== ASR 用户层调用示例 ===" << std::endl;
    std::cout << "🎯 目标：一行代码完成文件到ASR识别的完整流程" << std::endl;
    std::cout << "📁 音频文件: " << audioFile << std::endl;
    std::cout << "" << std::endl;
    
    // 创建管理器和回调
    Asr::AsrManager manager;
    EnhancedCallback callback;
    manager.setCallback(&callback);
    
    // ============================================================================
    // 异步调用 ASR
    // ============================================================================
    std::cout << "🚀 执行：manager.recognizeAudioFileAsync(audioFile)" << std::endl << std::endl;
    manager.recognizeAudioFileAsync(audioFile);

    // 等待识别完成...
    // 在实际应用中，这里可以处理其他任务，而不是阻塞等待
    std::cout << "⏳ 主线程等待识别完成..." << std::endl;
    while (manager.getStatus() != Asr::AsrStatus::DISCONNECTED) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    std::cout << "✅ 识别流程已结束" << std::endl << std::endl;

    
    return 0;
} 