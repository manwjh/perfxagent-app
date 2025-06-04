#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <audio/audio_manager.h>
#include <audio/audio_types.h>
#include <unistd.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <opus/opus.h>
#include <ogg/ogg.h>
#include <csignal>
#include <atomic>

// Default audio parameters
#define DEFAULT_SAMPLE_RATE SampleRate::RATE_48000
#define DEFAULT_CHANNELS ChannelCount::MONO
#define DEFAULT_FORMAT SampleFormat::FLOAT32
#define DEFAULT_FRAMES_PER_BUFFER 256

using namespace perfx::audio;

// 全局变量用于信号处理
std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "\n接收到信号 " << signum << std::endl;
    g_running = false;
}

// Helper function to convert float to int16
int16_t floatToInt16(float value) {
    // Clamp the value between -1.0 and 1.0
    value = std::max(-1.0f, std::min(1.0f, value));
    // Convert to int16_t
    return static_cast<int16_t>(value * 32767.0f);
}

// Helper function to print available sample rates
void printSampleRates() {
    std::cout << "Available sample rates:\n";
    std::cout << "1. 8000 Hz\n";
    std::cout << "2. 16000 Hz\n";
    std::cout << "3. 32000 Hz\n";
    std::cout << "4. 44100 Hz\n";
    std::cout << "5. 48000 Hz\n";
}

// Helper function to get supported sample rates from device
std::vector<int> getDeviceSupportedSampleRates(const DeviceInfo& device) {
    std::vector<int> supportedRates = {
        8000, 16000, 32000, 44100, 48000
    };
    std::vector<int> deviceRates;
    
    // Filter supported rates based on device's default sample rate
    for (int rate : supportedRates) {
        if (rate <= device.defaultSampleRate) {
            deviceRates.push_back(rate);
        }
    }
    return deviceRates;
}

// Helper function to print supported sample rates
void printSupportedSampleRates(const std::vector<int>& rates) {
    std::cout << "\nSupported sample rates for this device:\n";
    for (size_t i = 0; i < rates.size(); ++i) {
        std::cout << (i + 1) << ". " << rates[i] << " Hz\n";
    }
}

// Helper function to get sample rate from user selection
SampleRate getSampleRateFromUser(const std::vector<int>& supportedRates) {
    int choice;
    std::cout << "Select sample rate (1-" << supportedRates.size() << "): ";
    std::cin >> choice;

    if (choice > 0 && choice <= static_cast<int>(supportedRates.size())) {
        int selectedRate = supportedRates[choice - 1];
        return static_cast<SampleRate>(selectedRate);
    }

    std::cout << "Invalid choice, using default (48000 Hz)\n";
    return DEFAULT_SAMPLE_RATE;
}

// Helper function to get executable directory
std::string getExecutableDir() {
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        // Get the directory part of the path
        char* lastSlash = strrchr(path, '/');
        if (lastSlash != nullptr) {
            *lastSlash = '\0';
            return std::string(path);
        }
    }
    return ".";
}

// Helper function to generate output filename
std::string generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels) {
    std::string channelStr = (channels == ChannelCount::MONO) ? "mono" : "stereo";
    std::string extension = (format == "WAV") ? "wav" : "ogg";
    return std::string("recording_") + std::to_string(sampleRate) + "hz_" + channelStr + "." + extension;
}

// Helper function to get encoding format from user
EncodingFormat getEncodingFormatFromUser() {
    int choice;
    std::cout << "\nSelect encoding format:\n";
    std::cout << "1. WAV\n";
    std::cout << "2. OPUS\n";
    std::cout << "Enter your choice (1-2): ";
    std::cin >> choice;

    switch (choice) {
        case 1:
            return EncodingFormat::WAV;
        case 2:
            return EncodingFormat::OPUS;
        default:
            std::cout << "Invalid choice, using default (WAV)\n";
            return EncodingFormat::WAV;
    }
}

// Helper function to get Opus frame length from user
int getOpusFrameLength() {
    int choice;
    std::cout << "\nSelect Opus frame length (ms):\n";
    std::cout << "1. 20ms (default)\n";
    std::cout << "2. 40ms\n";
    std::cout << "3. 60ms\n";
    std::cout << "Enter your choice (1-3): ";
    std::cin >> choice;

    switch (choice) {
        case 1:
            return 20;
        case 2:
            return 40;
        case 3:
            return 60;
        default:
            std::cout << "Invalid choice, using default (20ms)\n";
            return 20;
    }
}

// Helper function to configure input settings
AudioConfig configureInputSettings(const std::vector<DeviceInfo>& devices) {
    AudioConfig config;
    
    // List available input devices
    std::cout << "\n=== Input Device Selection ===" << std::endl;
    std::vector<DeviceInfo> inputDevices;
    for (const auto& device : devices) {
        if (device.type == DeviceType::INPUT || device.type == DeviceType::BOTH) {
            std::cout << device.index << ": " << device.name 
                      << " (通道数: " << device.maxInputChannels 
                      << ", 默认采样率: " << device.defaultSampleRate << "Hz)" << std::endl;
            inputDevices.push_back(device);
        }
    }

    // Get input device selection
    int inputIdx = -1;
    std::cout << "Select input device index for recording: ";
    std::cin >> inputIdx;

    // Find selected device
    DeviceInfo selectedDevice;
    bool found = false;
    for (const auto& device : inputDevices) {
        if (device.index == inputIdx) {
            selectedDevice = device;
            found = true;
            break;
        }
    }

    if (!found) {
        std::cerr << "Invalid device index" << std::endl;
        exit(1);
    }

    // Display selected device details
    std::cout << "\nSelected Input Device:" << std::endl;
    std::cout << "  Name: " << selectedDevice.name << std::endl;
    std::cout << "  Index: " << selectedDevice.index << std::endl;
    std::cout << "  Default sample rate: " << selectedDevice.defaultSampleRate << std::endl;
    std::cout << "  Max input channels: " << selectedDevice.maxInputChannels << std::endl;
    std::cout << "  Default latency: " << selectedDevice.defaultLatency << std::endl;

    // Get supported sample rates and let user select one
    auto supportedRates = getDeviceSupportedSampleRates(selectedDevice);
    printSupportedSampleRates(supportedRates);
    SampleRate selectedSampleRate = getSampleRateFromUser(supportedRates);

    // Configure audio settings
    config.sampleRate = selectedSampleRate;
    config.channels = (selectedDevice.maxInputChannels > 1) ? ChannelCount::STEREO : ChannelCount::MONO;
    config.format = DEFAULT_FORMAT;
    config.framesPerBuffer = DEFAULT_FRAMES_PER_BUFFER;
    config.encodingFormat = EncodingFormat::WAV;  // Default to WAV
    config.opusFrameLength = 20;  // Default Opus frame length

    std::cout << "\n配置摘要:" << std::endl;
    std::cout << "输入设备: " << selectedDevice.name << std::endl;
    std::cout << "采样率: " << static_cast<int>(config.sampleRate) << "Hz" << std::endl;
    std::cout << "通道数: " << (config.channels == ChannelCount::MONO ? "单声道" : "立体声") << std::endl;
    std::cout << "输出格式: " << (config.encodingFormat == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;

    return config;
}

// 输出设置结构
struct OutputSettings {
    EncodingFormat format = EncodingFormat::WAV;  // 输出格式
    int opusFrameLength = 20;                     // Opus帧长度(ms)
    int opusBitrate = 64000;                      // Opus比特率
    int opusComplexity = 10;                      // Opus复杂度
    bool enableResampling = false;                // 是否启用重采样
    SampleRate targetSampleRate = SampleRate::RATE_48000; // 目标采样率
    std::string outputFile;                       // 输出文件名
};

// 配置输出设置
OutputSettings configureOutputSettings(const AudioConfig& inputConfig) {
    OutputSettings settings;
    
    std::cout << "\n=== Output Configuration ===" << std::endl;
    
    // 1. 配置音频处理参数
    std::cout << "Configure audio processing:" << std::endl;
    
    // 重采样配置
    std::cout << "Enable resampling? (y/n): ";
    char choice;
    std::cin >> choice;
    settings.enableResampling = (choice == 'y' || choice == 'Y');
    
    if (settings.enableResampling) {
        std::cout << "Select target sample rate:" << std::endl;
        printSampleRates();
        int rateChoice;
        std::cin >> rateChoice;
        switch (rateChoice) {
            case 1: settings.targetSampleRate = SampleRate::RATE_8000; break;
            case 2: settings.targetSampleRate = SampleRate::RATE_16000; break;
            case 3: settings.targetSampleRate = SampleRate::RATE_32000; break;
            case 4: settings.targetSampleRate = SampleRate::RATE_44100; break;
            case 5: settings.targetSampleRate = SampleRate::RATE_48000; break;
            default: settings.targetSampleRate = SampleRate::RATE_48000;
        }
    }

    // 2. 配置编码格式
    std::cout << "\nSelect encoding format:" << std::endl;
    std::cout << "1. WAV" << std::endl;
    std::cout << "2. OPUS" << std::endl;
    std::cout << "Enter your choice (1-2): ";
    int formatChoice;
    std::cin >> formatChoice;
    settings.format = (formatChoice == 2) ? EncodingFormat::OPUS : EncodingFormat::WAV;

    if (settings.format == EncodingFormat::OPUS) {
        // 3. 配置Opus参数
        std::cout << "\nConfigure Opus encoding:" << std::endl;
        
        // 3.1 帧长度
        std::cout << "Select Opus frame length (ms):" << std::endl;
        std::cout << "1. 20ms (default, best for voice)" << std::endl;
        std::cout << "2. 40ms (balanced)" << std::endl;
        std::cout << "3. 60ms (higher compression)" << std::endl;
        std::cout << "Enter your choice (1-3): ";
        int frameChoice;
        std::cin >> frameChoice;
        switch (frameChoice) {
            case 1: settings.opusFrameLength = 20; break;
            case 2: settings.opusFrameLength = 40; break;
            case 3: settings.opusFrameLength = 60; break;
            default: settings.opusFrameLength = 20;
        }

        // 3.2 比特率
        std::cout << "\nSelect Opus bitrate:" << std::endl;
        std::cout << "1. 16kbps (low quality, voice only)" << std::endl;
        std::cout << "2. 24kbps (medium quality, voice)" << std::endl;
        std::cout << "3. 32kbps (high quality, voice)" << std::endl;
        std::cout << "4. 48kbps (very high quality)" << std::endl;
        std::cout << "Enter your choice (1-4): ";
        int bitrateChoice;
        std::cin >> bitrateChoice;
        switch (bitrateChoice) {
            case 1: settings.opusBitrate = 16000; break;
            case 2: settings.opusBitrate = 24000; break;
            case 3: settings.opusBitrate = 32000; break;
            case 4: settings.opusBitrate = 48000; break;
            default: settings.opusBitrate = 32000;
        }

        // 3.3 复杂度
        std::cout << "\nSelect Opus complexity (1-10):" << std::endl;
        std::cout << "Higher values give better quality but use more CPU" << std::endl;
        std::cout << "Recommended: 6-8 for voice" << std::endl;
        std::cout << "Enter complexity (1-10): ";
        std::cin >> settings.opusComplexity;
        settings.opusComplexity = std::max(1, std::min(10, settings.opusComplexity));
    }

    // 4. 生成输出文件名
    settings.outputFile = generateOutputFilename(
        (settings.format == EncodingFormat::WAV) ? "WAV" : "OPUS",
        static_cast<int>(inputConfig.sampleRate),
        inputConfig.channels
    );

    // 5. 打印配置摘要
    std::cout << "\nOutput Configuration Summary:" << std::endl;
    std::cout << "- Output Format: " << (settings.format == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;
    if (settings.format == EncodingFormat::OPUS) {
        std::cout << "- OPUS Frame Length: " << settings.opusFrameLength << "ms" << std::endl;
        std::cout << "- OPUS Bitrate: " << settings.opusBitrate << "bps" << std::endl;
        std::cout << "- OPUS Complexity: " << settings.opusComplexity << std::endl;
    }
    std::cout << "- Sample Rate: " << static_cast<int>(inputConfig.sampleRate) << " Hz" << std::endl;
    std::cout << "- Resampling: " << (settings.enableResampling ? "Enabled" : "Disabled") << std::endl;
    if (settings.enableResampling) {
        std::cout << "- Target Sample Rate: " << static_cast<int>(settings.targetSampleRate) << " Hz" << std::endl;
    }
    std::cout << "- Output File: " << settings.outputFile << std::endl;

    return settings;
}

// 录音到文件
void recordToFile(AudioManager& manager, const OutputSettings& outputSettings) {
    std::cout << "\n开始录音，按Enter键停止...\n";
    
    // 获取可用设备
    auto devices = manager.getAvailableDevices();
    if (devices.empty()) {
        std::cerr << "No input devices available" << std::endl;
        return;
    }

    // 配置输入设置
    auto config = configureInputSettings(devices);
    
    // 初始化音频管理器
    if (!manager.initialize(config)) {
        std::cerr << "Failed to initialize audio manager" << std::endl;
        return;
    }

    // 更新管理器配置
    manager.updateConfig(config);

    // 开始录音
    if (!manager.startRecording(outputSettings.outputFile)) {
        std::cerr << "Failed to start recording" << std::endl;
        return;
    }

    // 等待用户按Enter键停止录音
    std::cin.ignore();  // 清除之前的输入
    std::cin.get();     // 等待Enter键

    // 停止录音
    if (!manager.stopRecording()) {
        std::cerr << "Failed to stop recording" << std::endl;
        return;
    }

    std::cout << "录音已停止" << std::endl;
}

int main() {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    
    try {
        // 1. 初始化音频管理器
        AudioManager manager;
        if (!manager.initialize(AudioConfig::getDefaultInputConfig())) {
            std::cerr << "初始化音频管理器失败" << std::endl;
            return 1;
        }
        
        // 2. 获取并显示可用设备
        auto devices = manager.getAvailableDevices();
        if (devices.empty()) {
            std::cerr << "未找到输入设备" << std::endl;
            return 1;
        }
        
        // 3. 配置输入设备
        std::cout << "\n=== 输入设备配置 ===" << std::endl;
        std::cout << "可用的输入设备:\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            std::cout << i << ". " << devices[i].name 
                      << " (通道数: " << devices[i].maxInputChannels 
                      << ", 默认采样率: " << devices[i].defaultSampleRate << "Hz)\n";
        }
        
        // 选择设备
        size_t deviceIndex;
        do {
            std::cout << "\n请选择输入设备 (0-" << devices.size() - 1 << "): ";
            std::string input;
            std::getline(std::cin, input);
            try {
                deviceIndex = std::stoul(input);
            } catch (const std::exception&) {
                deviceIndex = devices.size(); // 设置为无效值
            }
        } while (deviceIndex >= devices.size());
        
        // 获取选中的设备
        DeviceInfo selectedDevice = devices[deviceIndex];
        
        // 配置音频参数
        AudioConfig inputConfig = AudioConfig::getDefaultInputConfig();
        inputConfig.sampleRate = static_cast<SampleRate>(selectedDevice.defaultSampleRate);
        inputConfig.channels = (selectedDevice.maxInputChannels > 1) ? 
                             ChannelCount::STEREO : ChannelCount::MONO;
        inputConfig.framesPerBuffer = 256;  // 设置合理的缓冲区大小
        inputConfig.format = SampleFormat::FLOAT32;  // 使用浮点格式
        
        // 验证设备配置
        std::cout << "\n=== 设备配置验证 ===" << std::endl;
        std::cout << "设备: " << selectedDevice.name << std::endl;
        std::cout << "采样率: " << static_cast<int>(inputConfig.sampleRate) << "Hz" << std::endl;
        std::cout << "通道数: " << (inputConfig.channels == ChannelCount::STEREO ? "立体声" : "单声道") << std::endl;
        std::cout << "缓冲区大小: " << inputConfig.framesPerBuffer << " 帧" << std::endl;
        std::cout << "采样格式: " << (inputConfig.format == SampleFormat::FLOAT32 ? "32位浮点" : "16位整数") << std::endl;
        
        // 4. 配置输出设置
        std::cout << "\n=== 输出配置 ===" << std::endl;
        OutputSettings outputSettings = configureOutputSettings(inputConfig);
        
        // 5. 显示最终配置
        std::cout << "\n=== 最终配置 ===" << std::endl;
        std::cout << "输入设备: " << selectedDevice.name << std::endl;
        std::cout << "采样率: " << static_cast<int>(inputConfig.sampleRate) << "Hz" << std::endl;
        std::cout << "通道数: " << (inputConfig.channels == ChannelCount::STEREO ? "立体声" : "单声道") << std::endl;
        std::cout << "输出格式: " << (outputSettings.format == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;
        std::cout << "输出文件: " << outputSettings.outputFile << std::endl;
        
        // 6. 初始化设备并开始录音
        std::cout << "\n=== 开始录音 ===" << std::endl;
        std::cout << "[DEBUG] 更新音频配置..." << std::endl;
        if (!manager.updateConfig(inputConfig)) {
            std::cerr << "更新音频配置失败: " << manager.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "[DEBUG] 开始录音..." << std::endl;
        if (!manager.startRecording(outputSettings.outputFile)) {
            std::cerr << "开始录音失败: " << manager.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "录音已开始，按Enter键停止..." << std::endl;
        
        // 7. 等待用户停止录音
        std::cin.ignore();  // 清除之前的输入
        std::cin.get();     // 等待Enter键
        
        // 8. 停止录音
        std::cout << "[DEBUG] 停止录音..." << std::endl;
        if (!manager.stopRecording()) {
            std::cerr << "停止录音失败: " << manager.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "录音已停止，文件已保存: " << outputSettings.outputFile << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 