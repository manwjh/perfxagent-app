#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include "asr/asr_manager.h"
#include "audio/audio_manager.h"

using namespace Asr;
using namespace perfx::audio;

int main() {
    std::cout << "=== 实时ASR测试程序 ===" << std::endl;
    
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
    
    // 3. 创建音频管理器
    auto audioManager = std::make_unique<AudioManager>();
    
    // 4. 配置音频参数
    AudioConfig audioConfig;
    audioConfig.sampleRate = SampleRate::RATE_16000;
    audioConfig.channels = ChannelCount::MONO;
    audioConfig.format = SampleFormat::INT16;
    audioConfig.framesPerBuffer = 256;
    
    // 5. 初始化音频管理器
    if (!audioManager->initialize(audioConfig)) {
        std::cerr << "Failed to initialize audio manager" << std::endl;
        return -1;
    }
    
    // 6. 设置ASR管理器到音频管理器
    audioManager->setRealtimeAsrManager(asrManager.get());
    audioManager->setRealtimeAsrEnabled(true);
    
    // 7. 启动实时ASR流
    if (!asrManager->startRealtimeStreaming(audioConfig)) {
        std::cerr << "Failed to start realtime ASR streaming" << std::endl;
        return -1;
    }
    
    std::cout << "实时ASR流已启动，开始录音..." << std::endl;
    
    // 8. 启动音频流
    if (!audioManager->startStreamRecording("")) {
        std::cerr << "Failed to start audio stream" << std::endl;
        return -1;
    }
    
    // 9. 运行一段时间
    std::cout << "录音进行中，按Enter键停止..." << std::endl;
    std::cin.get();
    
    // 10. 停止录音
    audioManager->stopStreamRecording();
    asrManager->stopRealtimeStreaming();
    
    std::cout << "测试完成" << std::endl;
    return 0;
} 