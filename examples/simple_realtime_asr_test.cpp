#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include "asr/asr_manager.h"
#include "audio/audio_types.h"

using namespace Asr;
using namespace perfx::audio;

// 模拟音频数据生成
std::vector<int16_t> generateTestAudio(size_t frameCount) {
    std::vector<int16_t> audioData(frameCount);
    for (size_t i = 0; i < frameCount; ++i) {
        // 生成简单的正弦波测试音频
        audioData[i] = static_cast<int16_t>(32767 * 0.1 * sin(2 * M_PI * 440 * i / 16000));
    }
    return audioData;
}

int main() {
    std::cout << "=== 简化实时ASR测试程序 ===" << std::endl;
    
    // 1. 创建ASR管理器
    auto asrManager = std::make_unique<AsrManager>();
    
    // 2. 配置ASR参数
    AsrConfig config;
    config.serverUrl = "wss://your-asr-server.com/asr";
    config.appId = "your-app-id";
    config.secretKey = "your-secret-key";
    config.logLevel = ASR_LOG_INFO;
    config.enableBusinessLog = true;
    
    asrManager->setConfig(config);
    
    // 3. 配置音频参数
    AudioConfig audioConfig;
    audioConfig.sampleRate = SampleRate::RATE_16000;
    audioConfig.channels = ChannelCount::MONO;
    audioConfig.format = SampleFormat::INT16;
    audioConfig.framesPerBuffer = 256;
    
    // 7. 启动实时ASR流
    if (!asrManager->startRealtimeStreaming(audioConfig.sampleRate, audioConfig.channels, audioConfig.bitsPerSample)) {
        std::cerr << "Failed to start realtime ASR streaming" << std::endl;
        return -1;
    }
    
    std::cout << "实时ASR流已启动，开始发送测试音频..." << std::endl;
    
    // 5. 模拟音频数据发送
    const size_t framesPerPacket = 1600;  // 100ms @ 16kHz
    const size_t totalPackets = 10;        // 发送10个包
    
    for (size_t packet = 0; packet < totalPackets; ++packet) {
        // 生成测试音频数据
        auto audioData = generateTestAudio(framesPerPacket);
        
        // 发送到ASR
        if (asrManager->sendRealtimeAudio(audioData.data(), audioData.size())) {
            std::cout << "Packet " << (packet + 1) << "/" << totalPackets 
                      << " sent: " << audioData.size() << " frames" << std::endl;
        } else {
            std::cerr << "Failed to send packet " << (packet + 1) << std::endl;
        }
        
        // 等待100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 6. 停止ASR流
    asrManager->stopRealtimeStreaming();
    
    std::cout << "测试完成" << std::endl;
    return 0;
} 