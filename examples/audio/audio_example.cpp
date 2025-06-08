/**
 * @file audio_example.cpp
 * @brief PerfX音频录制工具示例程序
 * 
 * 本程序演示了如何使用PerfX音频模块进行音频录制，支持以下功能：
 * 1. 音频设备管理：自动检测和选择输入设备
 * 2. 音频参数配置：采样率、通道数、格式等
 * 3. 编码选项：支持WAV无损和OPUS有损压缩
 * 4. VAD语音活动检测：智能检测语音片段
 * 5. 配置文件管理：保存和加载录制配置
 */

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstring>
#include <audio/audio_manager.h>
#include <audio/audio_types.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <csignal>
#include <atomic>
#include <filesystem>
#include <nlohmann/json.hpp>

using namespace perfx::audio;

/**
 * @brief 音频帧大小配置
 * 
 * 音频帧大小(framesPerBuffer)的选择需要考虑以下因素：
 * 1. 延迟要求：较小的帧大小意味着更低的延迟
 * 2. CPU负载：较大的帧大小可以减少CPU调用频率
 * 3. 音频处理要求：需要与音频处理模块(如Opus编码器、RNNoise降噪器)的要求匹配
 * 
 * 当前配置使用240帧(5ms @ 48kHz)，这是基于以下考虑：
 * - 与Opus编码器的标准帧大小(2.5ms-60ms)兼容
 * - 与RNNoise降噪器的处理要求(480采样点)匹配
 * - 提供良好的实时性能(5ms延迟)
 * - 在大多数音频设备上都能稳定工作
 */
static const int DEFAULT_FRAMES_PER_BUFFER = 240;  // 5ms @ 48kHz

// =============================================================================
// 2. 文件操作函数
// =============================================================================

/**
 * @brief 获取可执行文件所在目录
 * @return 返回可执行文件所在目录的路径字符串
 * 
 * 使用macOS特定的API获取当前可执行文件的路径，并返回其所在目录
 */
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

/**
 * @brief 生成输出文件名
 * @param format 音频格式（"WAV"或"OPUS"）
 * @param channels 声道数
 * @return 生成的输出文件完整路径
 * 
 * 根据音频格式和声道数生成标准化的输出文件名，并确保输出目录存在
 */
std::string generateOutputFilename(const std::string& format, ChannelCount channels) {
    std::string channelStr = (channels == ChannelCount::MONO) ? "mono" : "stereo";
    std::string extension = (format == "WAV") ? "wav" : "ogg";
    
    // 创建输出目录
    std::filesystem::path outputDir = std::filesystem::current_path() / "recordings";
    if (!std::filesystem::exists(outputDir)) {
        try {
            std::filesystem::create_directories(outputDir);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create output directory: " << e.what() << std::endl;
            return "";
        }
    }
    
    // 生成文件名 (固定使用48K采样率)
    std::string filename = std::string("recording_48000hz_") + 
                          channelStr + "." + extension;
    
    return (outputDir / filename).string();
}

/**
 * @brief 显示当前配置摘要
 * @param inputConfig 输入音频配置
 * @param outputSettings 输出设置
 * 
 * 以易读的格式显示当前音频配置的详细信息，包括设备信息、音频参数、编码设置和VAD状态
 */
void displayConfigSummary(const AudioConfig& inputConfig, const OutputSettings& outputSettings) {
    std::cout << "\n=== Current Configuration Summary ===" << std::endl;
    
    // 设备信息
    std::cout << "📱 Device Information:" << std::endl;
    std::cout << "  - Name: " << inputConfig.inputDevice.name << std::endl;
    std::cout << "  - Index: " << inputConfig.inputDevice.index << std::endl;
    std::cout << "  - Channels: " << inputConfig.inputDevice.maxInputChannels << std::endl;
    
    // 音频参数
    std::cout << "\n🎵 Audio Parameters:" << std::endl;
    int actualSampleRate = static_cast<int>(inputConfig.sampleRate);
    if (actualSampleRate == 48000) {
        std::cout << "  - Sample Rate: 48000 Hz (default)" << std::endl;
    } else {
        std::cout << "  - Sample Rate: " << actualSampleRate << " Hz (custom from JSON config)" << std::endl;
    }
    std::cout << "  - Channels: " << (inputConfig.channels == ChannelCount::MONO ? "Mono" : "Stereo") << std::endl;
    std::cout << "  - Format: " << (inputConfig.format == SampleFormat::FLOAT32 ? "Float32" : 
                                  inputConfig.format == SampleFormat::INT16 ? "Int16" :
                                  inputConfig.format == SampleFormat::INT24 ? "Int24" : "Unknown") << std::endl;
    std::cout << "  - Frames per buffer: " << inputConfig.framesPerBuffer << std::endl;
    
    // 编码参数
    std::cout << "\n🔧 Encoding Parameters:" << std::endl;
    std::cout << "  - Format: " << (outputSettings.format == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;
    if (outputSettings.format == EncodingFormat::OPUS) {
        // 查找应用类型描述
        const char* appDesc = "Unknown";
        for (const auto& opt : OPUS_APPLICATION_OPTIONS) {
            if (opt.opusApplication == outputSettings.opusApplication) {
                appDesc = opt.description;
                break;
            }
        }
        std::cout << "  - Application: " << appDesc << std::endl;
        std::cout << "  - Frame Length: " << outputSettings.opusFrameLength << "ms" << std::endl;
        std::cout << "  - Bitrate: " << outputSettings.opusBitrate << " bps (" << (outputSettings.opusBitrate/1000) << "kbps)" << std::endl;
        std::cout << "  - Complexity: " << outputSettings.opusComplexity << std::endl;
    }
    
    // VAD状态
    std::cout << "\n🎤 VAD Configuration:" << std::endl;
    std::cout << "  - Status: " << (inputConfig.vadConfig.enabled ? "✓ Enabled" : "✗ Disabled") << std::endl;
    if (inputConfig.vadConfig.enabled) {
        std::cout << "  - Threshold: " << inputConfig.vadConfig.threshold << " (adjustable in JSON config)" << std::endl;
        std::cout << "  - Silence timeout: " << inputConfig.vadConfig.silenceTimeoutMs << "ms (adjustable in JSON config)" << std::endl;
        std::cout << "  - Advanced features: " 
                  << (inputConfig.vadConfig.enableSilenceFrame ? "SilenceFrame " : "")
                  << (inputConfig.vadConfig.enableSentenceDetection ? "SentenceDetection " : "")
                  << (inputConfig.vadConfig.enableIdleDetection ? "IdleDetection" : "") << std::endl;
    } else {
        std::cout << "  - Note: All VAD features are disabled for continuous recording" << std::endl;
    }
    
    // 输出信息
    std::cout << "\n💾 Output Configuration:" << std::endl;
    std::cout << "  - File: " << outputSettings.outputFile << std::endl;
    std::cout << "  - Resampling: Auto (handled by system when needed)" << std::endl;
    
    std::cout << std::endl;
}

/**
 * @brief 显示可用音频设备列表
 * @param devices 音频设备列表
 * 
 * 以表格形式显示所有可用的音频设备，包括设备名称、索引、通道数和采样率等信息
 */
void displayDevices(const std::vector<DeviceInfo>& devices) {
    std::cout << "\n=== 可用音频设备 ===" << std::endl;
    std::cout << std::left << std::setw(4) << "索引" 
              << std::setw(40) << "设备名称" 
              << std::setw(8) << "类型" 
              << std::setw(8) << "输入通道" 
              << std::setw(8) << "输出通道" 
              << std::setw(8) << "采样率" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    
    for (const auto& device : devices) {
        std::cout << std::left << std::setw(4) << device.index 
                  << std::setw(40) << (device.name.length() > 37 ? device.name.substr(0, 37) + "..." : device.name)
                  << std::setw(8) << (device.type == DeviceType::INPUT ? "输入" : 
                                    device.type == DeviceType::OUTPUT ? "输出" : "双向")
                  << std::setw(8) << device.maxInputChannels
                  << std::setw(8) << device.maxOutputChannels
                  << std::setw(8) << device.defaultSampleRate << std::endl;
    }
    std::cout << std::endl;
}

/**
 * @brief 选择音频设备
 * @param devices 可用设备列表
 * @return 选中的设备索引
 * 
 * 通过用户交互选择音频设备，支持通过索引或名称进行选择
 */
int selectDevice(const std::vector<DeviceInfo>& devices) {
    int selectedIndex = -1;
    std::string input;
    
    while (selectedIndex < 0 || selectedIndex >= static_cast<int>(devices.size())) {
        std::cout << "\n请选择输入设备 (0-" << devices.size() - 1 << "): ";
        std::getline(std::cin, input);
        
        // 尝试通过索引选择
        try {
            selectedIndex = std::stoi(input);
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(devices.size())) {
                break;
            }
        } catch (const std::exception&) {
            // 如果转换失败，尝试通过名称选择
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i].name.find(input) != std::string::npos) {
                    selectedIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(devices.size())) {
            std::cout << "无效的选择，请重试" << std::endl;
        }
    }
    
    return selectedIndex;
}

/**
 * @brief 配置VAD参数
 * @param vadConfig VAD配置（输出参数）
 * 
 * 通过用户交互配置VAD（语音活动检测）参数，包括启用状态、阈值和超时时间
 */
void configureVADParameters(VADConfig& vadConfig) {
    std::cout << "\n=== 配置VAD参数 ===" << std::endl;
    
    // 配置VAD启用状态
    std::cout << "是否启用VAD (语音活动检测)?" << std::endl;
    std::cout << "1. 启用 (推荐)" << std::endl;
    std::cout << "2. 禁用 (连续录制)" << std::endl;
    
    int choice;
    do {
        std::cout << "请选择 (1-2): ";
        std::cin >> choice;
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    } while (choice < 1 || choice > 2);
    
    vadConfig.enabled = (choice == 1);
    
    if (vadConfig.enabled) {
        // 配置VAD阈值
        std::cout << "\n配置VAD阈值 (0.0-1.0):" << std::endl;
        std::cout << "推荐值: 0.5" << std::endl;
        
        do {
            std::cout << "请输入阈值: ";
            std::cin >> vadConfig.threshold;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (vadConfig.threshold < 0.0f || vadConfig.threshold > 1.0f);
        
        // 配置静音超时
        std::cout << "\n配置静音超时时间 (毫秒):" << std::endl;
        std::cout << "推荐值: 1000" << std::endl;
        
        do {
            std::cout << "请输入超时时间: ";
            std::cin >> vadConfig.silenceTimeoutMs;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (vadConfig.silenceTimeoutMs < 100 || vadConfig.silenceTimeoutMs > 5000);
        
        // 配置句子超时
        std::cout << "\n配置句子超时时间 (毫秒):" << std::endl;
        std::cout << "推荐值: 500" << std::endl;
        
        do {
            std::cout << "请输入超时时间: ";
            std::cin >> vadConfig.sentenceTimeoutMs;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (vadConfig.sentenceTimeoutMs < 100 || vadConfig.sentenceTimeoutMs > 2000);
        
        // 配置高级功能
        std::cout << "\n配置高级功能:" << std::endl;
        std::cout << "1. 启用静音帧检测" << std::endl;
        std::cout << "2. 启用句子检测" << std::endl;
        std::cout << "3. 启用空闲检测" << std::endl;
        std::cout << "4. 全部启用" << std::endl;
        std::cout << "5. 全部禁用" << std::endl;
        
        do {
            std::cout << "请选择 (1-5): ";
            std::cin >> choice;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (choice < 1 || choice > 5);
        
        switch (choice) {
            case 1:
                vadConfig.enableSilenceFrame = true;
                vadConfig.enableSentenceDetection = false;
                vadConfig.enableIdleDetection = false;
                break;
            case 2:
                vadConfig.enableSilenceFrame = false;
                vadConfig.enableSentenceDetection = true;
                vadConfig.enableIdleDetection = false;
                break;
            case 3:
                vadConfig.enableSilenceFrame = false;
                vadConfig.enableSentenceDetection = false;
                vadConfig.enableIdleDetection = true;
                break;
            case 4:
                vadConfig.enableSilenceFrame = true;
                vadConfig.enableSentenceDetection = true;
                vadConfig.enableIdleDetection = true;
                break;
            case 5:
                vadConfig.enableSilenceFrame = false;
                vadConfig.enableSentenceDetection = false;
                vadConfig.enableIdleDetection = false;
                break;
        }
    }
    
    std::cout << "\n✓ VAD参数配置完成" << std::endl;
}

// =============================================================================
// 1. 输入部分的设定 (Input Configuration Functions)
// =============================================================================

// Helper function to print available sample rates for device selection
void printSampleRates() {
    std::cout << "Available sample rates:\n";
    std::cout << "1. 8000 Hz\n";
    std::cout << "2. 16000 Hz\n";
    std::cout << "3. 32000 Hz\n";
    std::cout << "4. 44100 Hz\n";
    std::cout << "5. 48000 Hz\n";
}

// Note: Sample rate selection functions removed as we now use fixed 48K sample rate
// The system will automatically handle resampling if device doesn't support 48K

// Helper function to get default VAD configuration
VADConfig getDefaultVADConfig() {
    VADConfig vadConfig;
    vadConfig.enabled = false;
    vadConfig.threshold = 0.3f;              // 默认检测阈值 (0.0-1.0)
    vadConfig.silenceTimeoutMs = 1500;       // 默认静音超时 1.5秒
    vadConfig.sentenceTimeoutMs = 800;       // 默认句子间隔 0.8秒
    vadConfig.enableSilenceFrame = true;     // 默认启用静音帧检测
    vadConfig.enableSentenceDetection = true; // 默认启用句子检测
    vadConfig.enableIdleDetection = true;    // 默认启用空闲检测
    return vadConfig;
}

// Helper function to get VAD choice from user (simple on/off)
bool getVADChoice() {
    std::cout << "\n=== Voice Activity Detection (VAD) ===" << std::endl;
    std::cout << "Enable VAD (automatic silence detection)?" << std::endl;
    std::cout << "- ON: Automatically detect and handle silence periods" << std::endl;
    std::cout << "- OFF: Record continuously without silence detection" << std::endl;
    std::cout << "Note: Detailed VAD parameters are configured in the JSON config file" << std::endl;
    
    char choice;
    std::cout << "Enable VAD? (y/n): ";
    std::cin >> choice;
    
    bool vadEnabled = (choice == 'y' || choice == 'Y');
    std::cout << "VAD: " << (vadEnabled ? "✓ Enabled" : "✗ Disabled") << std::endl;
    
    if (vadEnabled) {
        std::cout << "VAD will use default parameters from configuration file." << std::endl;
        std::cout << "You can modify detailed VAD settings in audio_config.json after recording." << std::endl;
    }
    
    return vadEnabled;
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
    std::cout << "  Device sample rate: " << selectedDevice.defaultSampleRate << " Hz" << std::endl;
    std::cout << "  Max input channels: " << selectedDevice.maxInputChannels << std::endl;
    std::cout << "  Default latency: " << selectedDevice.defaultLatency << std::endl;
    std::cout << "  Recording sample rate: 48000 Hz (default)" << std::endl;

    // Configure audio settings
    config.sampleRate = SampleRate::RATE_48000;  // 默认使用48K采样率
    config.channels = (selectedDevice.maxInputChannels > 1) ? ChannelCount::STEREO : ChannelCount::MONO;
    config.framesPerBuffer = DEFAULT_FRAMES_PER_BUFFER;  // 使用标准帧大小(5ms @ 48kHz)，以匹配opus和rnnoise的要求
    config.format = SampleFormat::INT16;  // 使用INT16格式
    config.encodingFormat = EncodingFormat::OPUS;  // 使用OPUS编码
    config.opusFrameLength = 20;  // 20ms帧长
    config.opusBitrate = 32000;   // 32kbps比特率
    config.opusComplexity = 6;    // 中等复杂度
    config.inputDevice = selectedDevice;  // 设置选择的设备

    // 配置VAD - 简化用户选择为开关，详细参数使用默认值
    bool vadEnabled = getVADChoice();
    config.enableVAD = vadEnabled;
    config.vadConfig = getDefaultVADConfig();
    config.vadConfig.enabled = vadEnabled;  // 设置用户选择的开关状态

    std::cout << "\n配置摘要:" << std::endl;
    std::cout << "输入设备: " << selectedDevice.name << std::endl;
    std::cout << "采样率: 48000 Hz (默认)" << std::endl;
    std::cout << "通道数: " << (config.channels == ChannelCount::MONO ? "单声道" : "立体声") << std::endl;
    std::cout << "输出格式: " << (config.encodingFormat == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;
    std::cout << "VAD状态: " << (config.vadConfig.enabled ? "开启" : "关闭") << std::endl;

    return config;
}

// =============================================================================
// 2. 编码部分的设定 (Encoding Configuration Functions)
// =============================================================================

// Helper function to get encoding format from user
EncodingFormat getEncodingFormatFromUser() {
    int choice;
    std::cout << "\nSelect encoding format:\n";
    std::cout << "1. WAV (uncompressed)\n";
    std::cout << "2. OPUS (compressed)\n";
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

// Helper function to get Opus frame length from user (using libopus standards)
int getOpusFrameLength() {
    std::cout << "\nSelect Opus frame length:\n";
    const size_t numOptions = sizeof(OPUS_FRAME_OPTIONS) / sizeof(OPUS_FRAME_OPTIONS[0]);
    
    for (size_t i = 0; i < numOptions; ++i) {
        std::cout << (i + 1) << ". " << OPUS_FRAME_OPTIONS[i].description << std::endl;
    }
    
    int choice;
    std::cout << "Enter your choice (1-" << numOptions << "): ";
    std::cin >> choice;

    if (choice > 0 && choice <= static_cast<int>(numOptions)) {
        int frameLength = OPUS_FRAME_OPTIONS[choice - 1].lengthMs;
        std::cout << "Selected: " << frameLength << "ms (" 
                  << OPUS_FRAME_OPTIONS[choice - 1].samples48k << " samples @ 48kHz)" << std::endl;
        return frameLength;
    }

    std::cout << "Invalid choice, using default (20ms)\n";
    return 20;
}

// Helper function to get Opus bitrate from user (using libopus standards)
int getOpusBitrate() {
    std::cout << "\nSelect Opus bitrate:\n";
    const size_t numOptions = sizeof(OPUS_BITRATE_OPTIONS) / sizeof(OPUS_BITRATE_OPTIONS[0]);
    
    for (size_t i = 0; i < numOptions; ++i) {
        std::cout << (i + 1) << ". " << OPUS_BITRATE_OPTIONS[i].description << std::endl;
    }
    
    int choice;
    std::cout << "Enter your choice (1-" << numOptions << "): ";
    std::cin >> choice;

    if (choice > 0 && choice <= static_cast<int>(numOptions)) {
        int bitrate = OPUS_BITRATE_OPTIONS[choice - 1].bitrate;
        std::cout << "Selected: " << bitrate << " bps" << std::endl;
        return bitrate;
    }

    std::cout << "Invalid choice, using default (32kbps)\n";
    return 32000;
}

// Helper function to get Opus application type from user
int getOpusApplication() {
    std::cout << "\nSelect Opus application optimization:\n";
    const size_t numOptions = sizeof(OPUS_APPLICATION_OPTIONS) / sizeof(OPUS_APPLICATION_OPTIONS[0]);
    
    for (size_t i = 0; i < numOptions; ++i) {
        std::cout << (i + 1) << ". " << OPUS_APPLICATION_OPTIONS[i].description << std::endl;
    }
    
    int choice;
    std::cout << "Enter your choice (1-" << numOptions << "): ";
    std::cin >> choice;

    if (choice > 0 && choice <= static_cast<int>(numOptions)) {
        int application = OPUS_APPLICATION_OPTIONS[choice - 1].opusApplication;
        std::cout << "Selected: " << OPUS_APPLICATION_OPTIONS[choice - 1].description << std::endl;
        return application;
    }

    std::cout << "Invalid choice, using default (VOIP)\n";
    return 2048; // OPUS_APPLICATION_VOIP
}

// Helper function to get Opus complexity from user (using libopus valid range)
int getOpusComplexity() {
    std::cout << "\nSelect Opus complexity (0-10):\n";
    std::cout << "0 = Fastest encoding (lowest CPU usage)\n";
    std::cout << "5 = Balanced (recommended for real-time)\n"; 
    std::cout << "10 = Best quality (highest CPU usage)\n";
    std::cout << "Note: Higher values give better quality but use more CPU\n";
    std::cout << "Enter complexity (0-10): ";
    
    int complexity;
    std::cin >> complexity;
    
    // 验证范围 - libopus supports 0-10
    if (complexity >= 0 && complexity <= 10) {
        std::cout << "Selected complexity: " << complexity << std::endl;
        return complexity;
    }
    
    std::cout << "Invalid choice (must be 0-10), using default (5)\n";
    return 5;  // 中等复杂度
}

// =============================================================================
// 3. 输出部分的设定 (Output Configuration Functions)
// =============================================================================

// 配置输出设置
OutputSettings configureOutputSettings(const AudioConfig& inputConfig) {
    OutputSettings settings;
    
    std::cout << "\n=== Output Configuration ===" << std::endl;
    
    // 1. 配置编码格式
    std::cout << "Select encoding format:" << std::endl;
    std::cout << "1. WAV (uncompressed)" << std::endl;
    std::cout << "2. OPUS (compressed)" << std::endl;
    std::cout << "Enter your choice (1-2): ";
    int formatChoice;
    std::cin >> formatChoice;
    settings.format = (formatChoice == 2) ? EncodingFormat::OPUS : EncodingFormat::WAV;

    if (settings.format == EncodingFormat::OPUS) {
        // 2. 配置Opus参数（使用libopus标准值）
        std::cout << "\n=== Opus Encoding Configuration ===" << std::endl;
        
        // 2.1 应用类型（决定编码器优化目标）
        settings.opusApplication = getOpusApplication();
        
        // 2.2 帧长度
        settings.opusFrameLength = getOpusFrameLength();

        // 2.3 比特率
        settings.opusBitrate = getOpusBitrate();

        // 2.4 复杂度
        settings.opusComplexity = getOpusComplexity();
    }

    // 3. 生成输出文件名
    settings.outputFile = generateOutputFilename(
        (settings.format == EncodingFormat::WAV) ? "WAV" : "OPUS",
        inputConfig.channels
    );

    // 4. 打印配置摘要
    std::cout << "\n=== Output Configuration Summary ===" << std::endl;
    std::cout << "- Output Format: " << (settings.format == EncodingFormat::WAV ? "WAV (uncompressed)" : "OPUS (compressed)") << std::endl;
    if (settings.format == EncodingFormat::OPUS) {
        // 查找应用类型描述
        const char* appDesc = "Unknown";
        for (const auto& opt : OPUS_APPLICATION_OPTIONS) {
            if (opt.opusApplication == settings.opusApplication) {
                appDesc = opt.description;
                break;
            }
        }
        std::cout << "- OPUS Application: " << appDesc << std::endl;
        std::cout << "- OPUS Frame Length: " << settings.opusFrameLength << "ms" << std::endl;
        std::cout << "- OPUS Bitrate: " << settings.opusBitrate << " bps (" << (settings.opusBitrate/1000) << "kbps)" << std::endl;
        std::cout << "- OPUS Complexity: " << settings.opusComplexity << " (0=fastest, 10=best quality)" << std::endl;
    }
    std::cout << "- Sample Rate: 48000 Hz (fixed default)" << std::endl;
    std::cout << "- Resampling: Auto-handled by system when needed" << std::endl;
    std::cout << "- Output File: " << settings.outputFile << std::endl;

    return settings;
}


// =============================================================================
// 5. 全局变量、信号处理和主函数 (Global Variables, Signal Handling & Main)
// =============================================================================

// 全局变量用于信号处理
std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "\n接收到信号 " << signum << std::endl;
    g_running = false;
}

int main() {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    
    try {
        std::cout << "🎙️  PerfX Audio Recording Tool" << std::endl;
        std::cout << "================================" << std::endl;
        
        // 1. 初始化音频管理器
        AudioManager manager;
        
        // 2. 获取并显示可用设备
        auto devices = manager.getAvailableDevices();
        if (devices.empty()) {
            AUDIO_LOG("No input devices found");
            return 1;
        }
        
        // 3. 检查是否存在配置文件
        std::string configPath = getExecutableDir() + "/audio_config.json";
        AudioConfig inputConfig;
        OutputSettings outputSettings;
        bool useExistingConfig = false;
        
        if (std::filesystem::exists(configPath)) {
            std::cout << "\n📁 Found existing audio configuration file!" << std::endl;
            std::cout << "Do you want to:" << std::endl;
            std::cout << "1. Use existing configuration (quick start)" << std::endl;
            std::cout << "2. Enter interactive configuration" << std::endl;
            
            int choice;
            std::cout << "Enter your choice (1-2): ";
            std::cin >> choice;
            std::cin.ignore(); // 清除输入缓冲区
            
            if (choice == 1) {
                if (manager.loadAudioConfig(inputConfig, outputSettings, configPath)) {
                    useExistingConfig = true;
                    std::cout << "\n✅ Successfully loaded existing configuration!" << std::endl;
                    
                    // 显示加载的配置摘要
                    std::cout << "\n📋 Configuration Summary:" << std::endl;
                    std::cout << "  🎤 Device: " << inputConfig.inputDevice.name << std::endl;
                    int loadedSampleRate = static_cast<int>(inputConfig.sampleRate);
                    if (loadedSampleRate == 48000) {
                        std::cout << "  🎵 Sample Rate: 48000 Hz (default)" << std::endl;
                    } else {
                        std::cout << "  🎵 Sample Rate: " << loadedSampleRate << " Hz (custom)" << std::endl;
                    }
                    std::cout << "  🔧 Encoding: " << (outputSettings.format == EncodingFormat::WAV ? "WAV" : "OPUS") << std::endl;
                    if (outputSettings.format == EncodingFormat::OPUS) {
                        std::cout << "  📊 Bitrate: " << (outputSettings.opusBitrate/1000) << "kbps" << std::endl;
                    }
                    std::cout << "  💾 Output: " << std::filesystem::path(outputSettings.outputFile).filename().string() << std::endl;
                    
                } else {
                    std::cout << "\n❌ Failed to load existing configuration, entering interactive mode..." << std::endl;
                }
            } else {
                std::cout << "\n🔧 Entering interactive configuration..." << std::endl;
            }
        } else {
            std::cout << "\n🔧 No configuration file found, entering interactive configuration..." << std::endl;
        }
        
        if (!useExistingConfig) {
            // 4. 配置输入设备（仅在不使用现有配置时）
            AUDIO_LOG("\n=== Input Device Configuration ===");
            AUDIO_LOG("Available input devices:");
            for (size_t i = 0; i < devices.size(); ++i) {
                AUDIO_LOG(i << ". " << devices[i].name 
                         << " (channels: " << devices[i].maxInputChannels 
                         << ", sample rate: " << devices[i].defaultSampleRate << "Hz)");
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
            inputConfig = AudioConfig::getDefaultInputConfig();
            inputConfig.sampleRate = SampleRate::RATE_48000;  // 默认使用48K采样率
            inputConfig.channels = (selectedDevice.maxInputChannels > 1) ? ChannelCount::STEREO : ChannelCount::MONO;
            inputConfig.framesPerBuffer = DEFAULT_FRAMES_PER_BUFFER;  // 使用标准帧大小(5ms @ 48kHz)，以匹配opus和rnnoise的要求
            inputConfig.format = SampleFormat::INT16;  // 使用INT16格式
            inputConfig.inputDevice = selectedDevice;  // 设置选择的设备
            inputConfig.encodingFormat = EncodingFormat::OPUS;  // 使用OPUS编码
            inputConfig.opusFrameLength = 20;  // 20ms帧长
            inputConfig.opusBitrate = 32000;   // 32kbps比特率
            inputConfig.opusComplexity = 6;    // 中等复杂度
            
            // 配置VAD - 简化用户选择为开关，详细参数使用默认值
            bool vadEnabled = getVADChoice();
            inputConfig.enableVAD = vadEnabled;
            inputConfig.vadConfig = getDefaultVADConfig();
            inputConfig.vadConfig.enabled = vadEnabled;  // 设置用户选择的开关状态
            
            // 配置输出设置
            outputSettings = configureOutputSettings(inputConfig);
        } else {
            // 使用现有配置时，需要验证设备是否仍然可用
            bool deviceFound = false;
            for (const auto& device : devices) {
                if (device.index == inputConfig.inputDevice.index && 
                    device.name == inputConfig.inputDevice.name) {
                    deviceFound = true;
                    inputConfig.inputDevice = device; // 更新设备信息
                    break;
                }
            }
            
            if (!deviceFound) {
                std::cout << "\n⚠️  Warning: Previously configured device not found!" << std::endl;
                std::cout << "Device: " << inputConfig.inputDevice.name << " (index: " << inputConfig.inputDevice.index << ")" << std::endl;
                std::cout << "Please select a new device:" << std::endl;
                
                for (size_t i = 0; i < devices.size(); ++i) {
                    std::cout << i << ". " << devices[i].name 
                             << " (channels: " << devices[i].maxInputChannels 
                             << ", sample rate: " << devices[i].defaultSampleRate << "Hz)" << std::endl;
                }
                
                size_t deviceIndex;
                do {
                    std::cout << "\n请选择输入设备 (0-" << devices.size() - 1 << "): ";
                    std::string input;
                    std::getline(std::cin, input);
                    try {
                        deviceIndex = std::stoul(input);
                    } catch (const std::exception&) {
                        deviceIndex = devices.size();
                    }
                } while (deviceIndex >= devices.size());
                
                inputConfig.inputDevice = devices[deviceIndex];
                std::cout << "✓ Device updated to: " << inputConfig.inputDevice.name << std::endl;
            }
            
            // 为现有配置生成新的输出文件名（避免覆盖）
            outputSettings.outputFile = generateOutputFilename(
                (outputSettings.format == EncodingFormat::WAV) ? "WAV" : "OPUS",
                inputConfig.channels
            );
        }

        // 保存配置到文件
        std::cout << "\n💾 Saving configuration..." << std::endl;
        if (manager.saveAudioConfig(inputConfig, outputSettings, configPath)) {
            std::cout << "✓ Configuration has been saved for future use." << std::endl;
        } else {
            std::cout << "⚠️  Warning: Failed to save configuration." << std::endl;
        }
        
        // 显示最终配置摘要
        displayConfigSummary(inputConfig, outputSettings);


        // 初始化音频管理器
        if (!manager.initialize(inputConfig)) {
            std::cerr << "初始化音频管理器失败" << std::endl;
            return 1;
        }

        // 开始录音
        std::cout << "\n开始录音..." << std::endl;
        if (!manager.startRecording(outputSettings.outputFile)) {
            std::string errorMsg = manager.getLastError();
            if (errorMsg.empty()) {
                errorMsg = "未知错误";
            }
            std::cerr << "启动录音失败: " << errorMsg << std::endl;
            return 1;
        }
        
        // 验证录音状态 - 通过检查输出文件是否存在
        if (!std::filesystem::exists(outputSettings.outputFile)) {
            std::cerr << "错误：录音文件未创建，录音可能未正确启动" << std::endl;
            return 1;
        }
        
        std::cout << "✓ 录音已开始，按Enter键停止..." << std::endl;
        std::cin.ignore();
        std::cin.get();
        
        // 添加停止录音的错误处理
        std::cout << "\n正在停止录音..." << std::endl;
        if (!manager.stopRecording()) {
            std::string errorMsg = manager.getLastError();
            if (errorMsg.empty()) {
                errorMsg = "未知错误";
            }
            std::cerr << "停止录音失败: " << errorMsg << std::endl;
            
            // 尝试重新初始化音频管理器
            std::cout << "尝试重新初始化音频系统..." << std::endl;
            manager.initialize(inputConfig);
            
            // 检查文件是否已经创建
            if (std::filesystem::exists(outputSettings.outputFile)) {
                std::cout << "录音文件已保存: " << outputSettings.outputFile << std::endl;
            } else {
                std::cerr << "警告: 录音文件可能未正确保存" << std::endl;
                return 1;
            }
        }
        
        // 验证文件是否成功创建
        if (!std::filesystem::exists(outputSettings.outputFile)) {
            std::cerr << "录音文件未创建: " << outputSettings.outputFile << std::endl;
            return 1;
        }
        
        // 显示编码结果
        std::cout << "\n=== Recording Results ===" << std::endl;
        std::cout << "  - File: " << outputSettings.outputFile << std::endl;
        std::cout << "  - Size: " << std::filesystem::file_size(outputSettings.outputFile) << " bytes" << std::endl;
        std::cout << "  - Status: ✓ Successfully recorded" << std::endl;
        
                
    } catch (const std::exception& e) {
        AUDIO_LOG("Error: " << e.what());
        return 1;
    }
    
    return 0;
} 