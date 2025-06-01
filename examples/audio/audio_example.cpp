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

// Default audio parameters
#define DEFAULT_SAMPLE_RATE SampleRate::RATE_48000
#define DEFAULT_CHANNELS ChannelCount::MONO
#define DEFAULT_FORMAT SampleFormat::FLOAT32
#define DEFAULT_FRAMES_PER_BUFFER 256

using namespace perfx::audio;

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

// 录音模块：将输入设备音频数据写入文件
void recordToFile(AudioManager& manager, const DeviceInfo& deviceToUse, const AudioConfig& config, 
                 const std::string& outputFile, EncodingFormat format, int opusFrameLength = 20) {
    // 更新 AudioManager 的配置
    if (!manager.updateConfig(config)) {
        std::cerr << "Failed to update audio manager configuration" << std::endl;
        return;
    }

    // 创建音频线程
    auto audioThread = manager.createAudioThread(config);
    if (!audioThread) {
        std::cerr << "Failed to create audio thread" << std::endl;
        return;
    }

    // 设置输入设备
    if (!audioThread->setInputDevice(deviceToUse)) {
        std::cerr << "Failed to set input device" << std::endl;
        return;
    }

    // 添加处理器并设置配置
    auto processor = manager.getProcessor();
    processor->initialize(config);  // 确保处理器使用正确的配置
    audioThread->addProcessor(processor);

    // 设置编码格式
    if (format == EncodingFormat::OPUS) {
        processor->setEncodingFormat(EncodingFormat::OPUS);
        processor->setOpusFrameLength(opusFrameLength);
    }

    // 设置输入回调
    std::vector<float> recordedAudio;
    audioThread->setInputCallback([&recordedAudio, &config](const void* input, size_t frames) {
        const float* data = static_cast<const float*>(input);
        size_t samples = frames * static_cast<int>(config.channels);
        recordedAudio.insert(recordedAudio.end(), data, data + samples);
    });

    // 开始录音
    if (!audioThread->start()) {
        std::cerr << "Failed to start recording" << std::endl;
        return;
    }

    // 等待用户按 Enter 停止录音
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    audioThread->stop();

    // 保存录音
    if (!recordedAudio.empty()) {
        bool success = false;
        if (format == EncodingFormat::WAV) {
            success = manager.writeWavFile(recordedAudio.data(), 
                                         recordedAudio.size() / static_cast<int>(config.channels), 
                                         outputFile);
        } else {
            success = manager.writeOpusFile(recordedAudio.data(), 
                                          recordedAudio.size() / static_cast<int>(config.channels), 
                                          outputFile);
        }

        if (success) {
            std::cout << "Recording saved to " << outputFile << std::endl;
        } else {
            std::cerr << "Failed to save recording" << std::endl;
        }
    } else {
        std::cerr << "No audio data recorded" << std::endl;
    }
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

int main() {
    auto& manager = AudioManager::getInstance();
    std::cout << "Initializing audio manager..." << std::endl;
    if (!manager.initialize()) {
        std::cerr << "Failed to initialize audio manager" << std::endl;
        return 1;
    }
    std::cout << "Audio manager initialized successfully" << std::endl;

    auto devices = manager.getAvailableDevices();
    std::cout << "\nTotal number of devices: " << devices.size() << std::endl;
    std::cout << "Device list details:" << std::endl;
    for (const auto& device : devices) {
        std::cout << "Device: " << device.name << std::endl;
        std::cout << "  Index: " << device.index << std::endl;
        std::cout << "  Type: " << (device.type == DeviceType::INPUT ? "INPUT" : 
                                  device.type == DeviceType::OUTPUT ? "OUTPUT" : "BOTH") << std::endl;
        std::cout << "  Default sample rate: " << device.defaultSampleRate << std::endl;
        std::cout << "  Max input channels: " << device.maxInputChannels << std::endl;
        std::cout << "  Max output channels: " << device.maxOutputChannels << std::endl;
        std::cout << "-------------------" << std::endl;
    }

    if (devices.empty()) {
        std::cerr << "No audio devices found" << std::endl;
        return 1;
    }

    // List available input devices
    std::cout << "\nAvailable audio input devices:\n";
    std::vector<DeviceInfo> inputDevices;
    for (const auto& device : devices) {
        if (device.type == DeviceType::INPUT || device.type == DeviceType::BOTH) {
            std::cout << device.index << ": " << device.name << "\n";
            inputDevices.push_back(device);
        }
    }

    // Get input device selection
    int inputIdx = -1;
    std::cout << "Select input device index for recording: ";
    std::cin >> inputIdx;

    std::cout << "\n=== Selected Device Details ===" << std::endl;
    std::cout << "User selected index: " << inputIdx << std::endl;
    std::cout << "Available input devices count: " << inputDevices.size() << std::endl;
    std::cout << "Input devices array contents:" << std::endl;
    for (size_t i = 0; i < inputDevices.size(); ++i) {
        std::cout << "  [" << i << "] Device index: " << inputDevices[i].index 
                  << ", Name: " << inputDevices[i].name << std::endl;
    }

    // 查找选中的设备
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
        return 1;
    }
    
    std::cout << "\nFound matching device:" << std::endl;
    std::cout << "  Index: " << selectedDevice.index << std::endl;
    std::cout << "  Name: " << selectedDevice.name << std::endl;
    std::cout << "  Type: " << (selectedDevice.type == DeviceType::INPUT ? "INPUT" : 
                               selectedDevice.type == DeviceType::OUTPUT ? "OUTPUT" : "BOTH") << std::endl;
    std::cout << "  Default sample rate: " << selectedDevice.defaultSampleRate << std::endl;
    std::cout << "  Max input channels: " << selectedDevice.maxInputChannels << std::endl;
    std::cout << "  Max output channels: " << selectedDevice.maxOutputChannels << std::endl;
    std::cout << "========================\n" << std::endl;
    
    // Get supported sample rates and let user select one
    auto supportedRates = getDeviceSupportedSampleRates(selectedDevice);
    printSupportedSampleRates(supportedRates);
    SampleRate selectedSampleRate = getSampleRateFromUser(supportedRates);
    
    // 使用设备的实际索引
    DeviceInfo deviceToUse = selectedDevice;
    std::cout << "\n=== Audio Configuration ===" << std::endl;
    std::cout << "Device name: " << deviceToUse.name << std::endl;
    std::cout << "Device index: " << deviceToUse.index << std::endl;
    std::cout << "Device type: " << (deviceToUse.type == DeviceType::INPUT ? "INPUT" : 
                                   deviceToUse.type == DeviceType::OUTPUT ? "OUTPUT" : "BOTH") << std::endl;
    std::cout << "Default sample rate: " << deviceToUse.defaultSampleRate << std::endl;
    std::cout << "Max input channels: " << deviceToUse.maxInputChannels << std::endl;

    // 使用用户选择的采样率
    std::cout << "Selected sample rate: " << static_cast<int>(selectedSampleRate) << " Hz" << std::endl;
    std::cout << "Channels: " << (DEFAULT_CHANNELS == ChannelCount::MONO ? "MONO" : "STEREO") << std::endl;
    std::cout << "Format: " << (DEFAULT_FORMAT == SampleFormat::FLOAT32 ? "FLOAT32" : "INT16") << std::endl;
    std::cout << "Frames per buffer: " << DEFAULT_FRAMES_PER_BUFFER << std::endl;
    std::cout << "========================\n" << std::endl;

    // 验证设备参数
    std::cout << "Validating device parameters..." << std::endl;
    if (deviceToUse.type != DeviceType::INPUT && deviceToUse.type != DeviceType::BOTH) {
        std::cerr << "Error: Selected device is not an input device" << std::endl;
        return 1;
    }

    std::cout << "Using selected sample rate: " << static_cast<int>(selectedSampleRate) << " Hz" << std::endl;

    AudioConfig config;
    config.sampleRate = selectedSampleRate;
    config.channels = DEFAULT_CHANNELS;
    config.format = DEFAULT_FORMAT;
    config.framesPerBuffer = DEFAULT_FRAMES_PER_BUFFER;
    config.inputDevice = deviceToUse;

    // 选择输出格式
    EncodingFormat selectedFormat = getEncodingFormatFromUser();
    std::string outputFormat = (selectedFormat == EncodingFormat::WAV) ? "WAV" : "OPUS";

    // 如果是OPUS格式，获取帧长度
    int opusFrameLength = 20; // 默认值
    if (selectedFormat == EncodingFormat::OPUS) {
        opusFrameLength = getOpusFrameLength();
    }

    // 生成包含采样率和通道信息的输出文件名
    std::string outputFile = generateOutputFilename(outputFormat, 
                                                  static_cast<int>(selectedSampleRate), 
                                                  config.channels);

    std::cout << "Starting recording to " << outputFile << "..." << std::endl;
    std::cout << "Configuration:\n";
    std::cout << "- Sample Rate: " << static_cast<int>(config.sampleRate) << " Hz\n";
    std::cout << "- Channels: " << (config.channels == ChannelCount::MONO ? "Mono" : "Stereo") << "\n";
    std::cout << "- Format: PCM 16-bit\n";
    std::cout << "- Output Format: " << outputFormat << std::endl;
    if (selectedFormat == EncodingFormat::OPUS) {
        std::cout << "- OPUS Frame Length: " << opusFrameLength << "ms" << std::endl;
    }
    
    std::cout << "\nPress Enter to start recording..." << std::endl;
    std::cin.ignore(); // 清除上次输入残留
    std::cin.get();

    std::cout << "Recording... Press Enter to stop." << std::endl;
    recordToFile(manager, selectedDevice, config, outputFile, selectedFormat, opusFrameLength);

    return 0;
} 