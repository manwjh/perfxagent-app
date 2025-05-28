#include "audio/audio_manager.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace perfx;

void printDevices(const std::vector<InputDeviceInfo>& devices) {
    std::cout << "Available input devices:" << std::endl;
    for (const auto& device : devices) {
        std::cout << "ID: " << device.id << ", Name: " << device.name << std::endl;
    }
}

int main() {
    // 获取音频管理器实例
    auto& audio = AudioManager::getInstance();

    // 初始化音频系统
    if (!audio.initialize()) {
        std::cerr << "Failed to initialize audio system: " << audio.getLastError() << std::endl;
        return 1;
    }

    // 获取并打印可用的输入设备
    auto inputDevices = audio.getInputDevices();
    printDevices(inputDevices);

    // 配置音频参数
    AudioConfig config;
    config.sampleRate = 16000;
    config.channels = 1;
    config.vadThreshold = 0.5f;
    audio.setConfig(config);

    // 设置回调函数
    AudioCallbacks callbacks;
    callbacks.onAudioData = [](const float* /*data*/, int frames) {
        // 处理音频数据
        std::cout << "Received " << frames << " frames of audio data" << std::endl;
    };

    callbacks.onVadStateChanged = [](bool isActive) {
        std::cout << "VAD state changed: " << (isActive ? "Active" : "Inactive") << std::endl;
    };

    callbacks.onStateChanged = [](AudioState state) {
        std::cout << "Audio state changed to: ";
        switch (state) {
            case AudioState::IDLE:
                std::cout << "IDLE";
                break;
            case AudioState::RECORDING:
                std::cout << "RECORDING";
                break;
            case AudioState::PLAYING:
                std::cout << "PLAYING";
                break;
            case AudioState::PAUSED:
                std::cout << "PAUSED";
                break;
        }
        std::cout << std::endl;
    };

    audio.setRecordingCallbacks(callbacks);

    // 选择第一个输入设备
    if (!inputDevices.empty()) {
        if (!audio.setInputDevice(inputDevices[0].id)) {
            std::cerr << "Failed to set input device: " << audio.getLastError() << std::endl;
            return 1;
        }
    }

    // 开始录音
    std::cout << "Starting recording..." << std::endl;
    if (!audio.startRecording()) {
        std::cerr << "Failed to start recording: " << audio.getLastError() << std::endl;
        return 1;
    }

    // 录音10秒
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // 停止录音
    std::cout << "Stopping recording..." << std::endl;
    audio.stopRecording();

    // 清理资源
    audio.cleanup();

    return 0;
} 