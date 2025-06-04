/**
 * @file audio_manager.cpp
 * @brief 音频管理器实现文件，负责音频设备、处理器和线程的管理
 */

#include "../../include/audio/audio_manager.h"
#include "../../include/audio/audio_types.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <opus/opus.h>
#include <ogg/ogg.h>
#include <cassert>
#include <sndfile.h>
#include <unistd.h>  // for getcwd
#include <limits.h>  // for PATH_MAX
#include <portaudio.h>
#include <mutex>
#include <filesystem>

namespace perfx {
namespace audio {

/**
 * @brief AudioManager的PIMPL实现类
 * 封装了AudioManager的具体实现细节
 */
class AudioManager::Impl {
public:
    AudioConfig config_;
    /**
     * @brief 构造函数
     * 初始化成员变量
     */
    Impl() : initialized_(false), isRecording_(false) {}

    /**
     * @brief 析构函数
     * 清理资源
     */
    ~Impl() {
        cleanup();
    }

    /**
     * @brief 初始化音频管理器
     * @return 初始化是否成功
     */
    bool initialize(const AudioConfig& config) {
        config_ = config;
        if (initialized_) {
            return true;
        }

        // 初始化 PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        processor_ = std::make_unique<AudioProcessor>();
        if (!processor_->initialize(config)) {
            std::cerr << "Failed to initialize audio processor" << std::endl;
            Pa_Terminate();
            return false;
        }

        thread_ = std::make_unique<AudioThread>();
        if (!thread_->initialize(processor_.get())) {
            std::cerr << "Failed to initialize audio thread" << std::endl;
            Pa_Terminate();
            return false;
        }

        initialized_ = true;
        return true;
    }

    /**
     * @brief 从配置文件加载配置
     * @param configPath 配置文件路径
     * @return 加载是否成功
     */
    bool loadConfig(const std::string& configPath) {
        try {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            nlohmann::json j;
            file >> j;

            AudioConfig config;
            config.sampleRate = static_cast<SampleRate>(j["sampleRate"].get<int>());
            config.channels = static_cast<ChannelCount>(j["channels"].get<int>());
            config.format = static_cast<SampleFormat>(j["format"].get<int>());
            config.framesPerBuffer = j["framesPerBuffer"].get<int>();
            config.encodingFormat = static_cast<EncodingFormat>(j["encodingFormat"].get<int>());
            config.opusFrameLength = j["opusFrameLength"].get<int>();

            return updateConfig(config);
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 保存配置到文件
     * @param configPath 配置文件路径
     * @return 保存是否成功
     */
    bool saveConfig(const std::string& configPath) {
        try {
            nlohmann::json j;
            j["sampleRate"] = static_cast<int>(config_.sampleRate);
            j["channels"] = static_cast<int>(config_.channels);
            j["format"] = static_cast<int>(config_.format);
            j["framesPerBuffer"] = config_.framesPerBuffer;
            j["encodingFormat"] = static_cast<int>(config_.encodingFormat);
            j["opusFrameLength"] = config_.opusFrameLength;

            std::ofstream file(configPath);
            if (!file.is_open()) {
                return false;
            }

            file << j.dump(4);
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    /**
     * @brief 获取可用的音频设备列表
     * @return 设备信息列表
     */
    std::vector<DeviceInfo> getAvailableDevices() {
        std::vector<DeviceInfo> devices;
        
        // 如果已经初始化，直接使用
        if (!initialized_) {
            PaError err = Pa_Initialize();
            if (err != paNoError) {
                std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
                return devices;
            }
        }

        // 获取设备数量
        int numDevices = Pa_GetDeviceCount();
        if (numDevices < 0) {
            std::cerr << "Error getting device count: " << Pa_GetErrorText(numDevices) << std::endl;
            if (!initialized_) {
                Pa_Terminate();
            }
            return devices;
        }

        // 枚举所有设备
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (!deviceInfo) continue;

            // 只添加输入设备
            if (deviceInfo->maxInputChannels > 0) {
                DeviceInfo device;
                device.index = i;
                device.name = deviceInfo->name;
                device.type = DeviceType::INPUT;
                device.maxInputChannels = deviceInfo->maxInputChannels;
                device.maxOutputChannels = deviceInfo->maxOutputChannels;
                device.defaultSampleRate = static_cast<int>(deviceInfo->defaultSampleRate);
                device.defaultLatency = deviceInfo->defaultLowInputLatency;
                devices.push_back(device);
            }
        }

        // 如果之前没有初始化，现在清理
        if (!initialized_) {
            Pa_Terminate();
        }
        return devices;
    }

    /**
     * @brief 创建音频处理线程
     * @param config 音频配置
     * @return 音频线程指针
     */
    std::shared_ptr<AudioThread> createAudioThread([[maybe_unused]] const AudioConfig& config) {
        auto thread = std::make_shared<AudioThread>();
        if (!thread->initialize(processor_.get())) {
            return nullptr;
        }
        return thread;
    }

    /**
     * @brief 获取音频处理器
     * @return 音频处理器指针
     */
    std::shared_ptr<AudioProcessor> getProcessor() {
        return processor_;
    }

    /**
     * @brief 更新音频配置
     * @param config 新的音频配置
     * @return 更新是否成功
     */
    bool updateConfig(const AudioConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // 验证配置
            if (!validateConfig(config)) {
                lastError_ = "Invalid audio configuration";
                return false;
            }
            
            // 如果设备已经打开，先关闭
            if (device_ && device_->isDeviceOpen()) {
                device_->closeDevice();
            }
            
            // 创建新的音频设备
            device_ = std::make_unique<AudioDevice>();
            if (!device_->initialize()) {
                lastError_ = "Failed to initialize audio device: " + device_->getLastError();
                return false;
            }
            
            // 获取当前设备信息
            auto devices = device_->getAvailableDevices();
            if (devices.empty()) {
                lastError_ = "No audio devices available";
                return false;
            }
            
            // 使用配置中指定的设备
            DeviceInfo selectedDevice;
            bool found = false;
            
            // 如果配置中指定了设备名称，则使用该设备
            if (!config.inputDevice.name.empty()) {
                for (const auto& device : devices) {
                    if (device.name == config.inputDevice.name) {
                        selectedDevice = device;
                        found = true;
                        break;
                    }
                }
            }
            
            // 如果没有找到指定设备或没有指定设备，使用默认设备
            if (!found) {
                // 查找 MacBook Pro 麦克风
                for (const auto& device : devices) {
                    if (device.name.find("MacBook Pro") != std::string::npos) {
                        selectedDevice = device;
                        found = true;
                        break;
                    }
                }
                
                // 如果还是没找到，使用第一个设备
                if (!found) {
                    selectedDevice = devices[0];
                }
            }
            
            // 打开设备
            std::cout << "[DEBUG] Opening device: " << selectedDevice.name << std::endl;
            std::cout << "[DEBUG] Configuration:" << std::endl;
            std::cout << "  - Sample Rate: " << static_cast<int>(config.sampleRate) << "Hz" << std::endl;
            std::cout << "  - Channels: " << static_cast<int>(config.channels) << std::endl;
            std::cout << "  - Format: " << static_cast<int>(config.format) << std::endl;
            std::cout << "  - Frames per buffer: " << config.framesPerBuffer << std::endl;
            
            if (!device_->openInputDevice(selectedDevice, config)) {
                lastError_ = "Failed to open input device: " + device_->getLastError();
                return false;
            }
            
            currentConfig_ = config;
            return true;
        } catch (const std::exception& e) {
            lastError_ = std::string("Exception in updateConfig: ") + e.what();
            return false;
        }
    }

    bool validateConfig(const AudioConfig& config) {
        // 验证采样率
        if (static_cast<int>(config.sampleRate) <= 0) {
            lastError_ = "Invalid sample rate: " + std::to_string(static_cast<int>(config.sampleRate));
            return false;
        }
        
        // 验证通道数
        if (config.channels != ChannelCount::MONO && config.channels != ChannelCount::STEREO) {
            lastError_ = "Invalid channel count: " + std::to_string(static_cast<int>(config.channels));
            return false;
        }
        
        // 验证缓冲区大小
        if (config.framesPerBuffer <= 0) {
            lastError_ = "Invalid frames per buffer: " + std::to_string(config.framesPerBuffer);
            return false;
        }
        
        // 验证采样格式
        if (config.format != SampleFormat::FLOAT32 && config.format != SampleFormat::INT16) {
            lastError_ = "Invalid sample format";
            return false;
        }
        
        return true;
    }

    /**
     * @brief 获取当前音频配置
     * @return 当前音频配置
     */
    const AudioConfig& getConfig() const {
        return config_;
    }

    /**
     * @brief 清理资源
     */
    void cleanup() {
        if (isRecording_) {
            stopRecording();
        }
        thread_.reset();
        processor_.reset();
        if (initialized_) {
            Pa_Terminate();
        }
        initialized_ = false;
    }

    /**
     * @brief 将音频数据写入WAV文件
     * @param input 输入音频数据
     * @param frames 帧数
     * @param filename 输出文件名
     * @return 写入是否成功
     */
    bool writeWavFile(const void* input, size_t frames, const std::string& filename) {
        if (!input || frames == 0) {
            lastError_ = "Invalid input parameters";
            return false;
        }

        try {
            // 检查文件路径
            std::filesystem::path filePath(filename);
            std::filesystem::path parentPath = filePath.parent_path();
            
            // 确保父目录存在
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                std::filesystem::create_directories(parentPath);
            }

            // 计算数据大小
            size_t dataSize = frames * sizeof(int16_t) * static_cast<int>(config_.channels);

            // 生成文件头
            WavHeader header = generateWavHeader(dataSize);

            // 写入文件头
            {
                std::ofstream file(filename, std::ios::binary);
                if (!file) {
                    lastError_ = "Failed to open file for writing header: " + filename;
                    return false;
                }
                file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
                if (!file.good()) {
                    lastError_ = "Failed to write WAV header";
                    return false;
                }
            }

            // 写入音频数据
            {
                std::ofstream file(filename, std::ios::binary | std::ios::app);
                if (!file) {
                    lastError_ = "Failed to open file for writing data: " + filename;
                    return false;
                }
                file.write(static_cast<const char*>(input), dataSize);
                if (!file.good()) {
                    lastError_ = "Failed to write audio data";
                    return false;
                }
            }

            return true;
        } catch (const std::exception& e) {
            lastError_ = std::string("Exception in writeWavFile: ") + e.what();
            return false;
        }
    }

    /**
     * @brief 将音频数据写入Opus文件
     * @param input 输入音频数据
     * @param frames 帧数
     * @param filename 输出文件名
     * @return 写入是否成功
     */
    bool writeOpusFile(const void* input, size_t frames, const std::string& filename) {
        if (!input || frames == 0) {
            std::cerr << "Invalid input parameters" << std::endl;
            return false;
        }

        try {
            // 检查文件路径
            std::filesystem::path filePath(filename);
            std::filesystem::path parentPath = filePath.parent_path();
            
            // 确保父目录存在
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                std::filesystem::create_directories(parentPath);
            }

            // 编码音频数据
            std::vector<std::vector<uint8_t>> encodedFrames;
            if (!processor_->encodeOpus(input, frames, encodedFrames)) {
                std::cerr << "Failed to encode audio data" << std::endl;
                return false;
            }

            // 打开文件
            FILE* file = fopen(filename.c_str(), "wb");
            if (!file) {
                std::cerr << "Failed to open Opus file for writing: " << filename << std::endl;
                return false;
            }

            // 初始化 OGG 流
            ogg_stream_state os;
            ogg_page og;
            ogg_packet op;

            if (ogg_stream_init(&os, rand()) != 0) {
                fclose(file);
                return false;
            }

            // 创建并写入 Opus 头
            std::vector<uint8_t> header;
            if (!processor_->createOpusHeader(header)) {
                ogg_stream_clear(&os);
                fclose(file);
                return false;
            }

            // 写入 Opus 头包
            op.packet = header.data();
            op.bytes = header.size();
            op.b_o_s = 1;  // 这是流的开始
            op.e_o_s = 0;  // 这不是流的结束
            op.granulepos = 0;
            op.packetno = 0;

            if (ogg_stream_packetin(&os, &op) != 0) {
                ogg_stream_clear(&os);
                fclose(file);
                return false;
            }

            // 写入 OpusTags 注释头
            const char* vendor = "perfxagent";
            const char* opusTags = "OpusTags";
            std::vector<uint8_t> tagsHeader;
            tagsHeader.reserve(16 + strlen(vendor) + 1);
            
            // 写入 "OpusTags" 标识
            tagsHeader.insert(tagsHeader.end(), opusTags, opusTags + 8);
            
            // 写入供应商字符串长度
            uint32_t vendorLength = strlen(vendor);
            tagsHeader.push_back(vendorLength & 0xFF);
            tagsHeader.push_back((vendorLength >> 8) & 0xFF);
            tagsHeader.push_back((vendorLength >> 16) & 0xFF);
            tagsHeader.push_back((vendorLength >> 24) & 0xFF);
            
            // 写入供应商字符串
            tagsHeader.insert(tagsHeader.end(), vendor, vendor + vendorLength);
            
            // 写入用户注释数量（0）
            uint32_t commentCount = 0;
            tagsHeader.push_back(commentCount & 0xFF);
            tagsHeader.push_back((commentCount >> 8) & 0xFF);
            tagsHeader.push_back((commentCount >> 16) & 0xFF);
            tagsHeader.push_back((commentCount >> 24) & 0xFF);

            op.packet = tagsHeader.data();
            op.bytes = tagsHeader.size();
            op.b_o_s = 0;  // 这不是流的开始
            op.e_o_s = 0;  // 这不是流的结束
            op.granulepos = 0;
            op.packetno = 1;

            if (ogg_stream_packetin(&os, &op) != 0) {
                ogg_stream_clear(&os);
                fclose(file);
                return false;
            }

            // 计算每帧的采样数
            int samplesPerFrame = static_cast<int>(config_.sampleRate) * config_.opusFrameLength / 1000;
            int64_t granulepos = 0;
            int packetno = 2;

            // 将编码数据分帧写入
            std::cout << "[DEBUG] Frame processing:" << std::endl;
            std::cout << "  - Total encoded frames: " << encodedFrames.size() << std::endl;
            std::cout << "  - Samples per frame: " << samplesPerFrame << std::endl;

            for (size_t i = 0; i < encodedFrames.size(); i++) {
                op.packet = encodedFrames[i].data();
                op.bytes = encodedFrames[i].size();
                op.b_o_s = 0;  // 这不是流的开始
                op.e_o_s = (i == encodedFrames.size() - 1);  // 最后一帧是流的结束
                op.granulepos = granulepos;
                op.packetno = packetno++;

                if (ogg_stream_packetin(&os, &op) != 0) {
                    std::cerr << "Failed to add packet to OGG stream" << std::endl;
                    ogg_stream_clear(&os);
                    fclose(file);
                    return false;
                }

                granulepos += samplesPerFrame;
                
                // 每写入一帧就刷新一次OGG流
                while (ogg_stream_flush(&os, &og)) {
                    if (fwrite(og.header, 1, static_cast<size_t>(og.header_len), file) != static_cast<size_t>(og.header_len) ||
                        fwrite(og.body, 1, static_cast<size_t>(og.body_len), file) != static_cast<size_t>(og.body_len)) {
                        std::cerr << "Failed to write OGG page" << std::endl;
                        ogg_stream_clear(&os);
                        fclose(file);
                        return false;
                    }
                }
            }

            // 确保所有数据都被写入
            if (fflush(file) != 0) {
                ogg_stream_clear(&os);
                fclose(file);
                return false;
            }

            ogg_stream_clear(&os);
            fclose(file);

            std::cout << "[DEBUG] Successfully wrote Opus file: " << filename << std::endl;
            std::cout << "  - Frames: " << frames << std::endl;
            std::cout << "  - Encoded size: " << encodedFrames.size() << " bytes" << std::endl;
            std::cout << "  - Samples per frame: " << samplesPerFrame << std::endl;
            std::cout << "  - Total packets: " << packetno << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Exception in writeOpusFile: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief 从WAV文件读取音频数据
     * @param filename 输入文件名
     * @param output 输出音频数据
     * @param frames 帧数
     * @return 读取是否成功
     */
    bool readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        // 读取WAV文件头
        WavHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // 验证WAV格式
        if (memcmp(header.riff, "RIFF", 4) != 0 ||
            memcmp(header.wave, "WAVE", 4) != 0 ||
            memcmp(header.fmt, "fmt ", 4) != 0 ||
            memcmp(header.data, "data", 4) != 0 ||
            header.format != 1) {
            return false;
        }

        // 读取数据
        std::vector<int16_t> pcmData(header.dataSize / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(pcmData.data()), header.dataSize);

        // 转换为float格式
        frames = pcmData.size() / header.channels;
        output.resize(pcmData.size());
        for (size_t i = 0; i < pcmData.size(); ++i) {
            output[i] = pcmData[i] / 32768.0f;
        }

        return true;
    }

    /**
     * @brief 生成输出文件名
     * @param format 文件格式
     * @param sampleRate 采样率
     * @param channels 声道数
     * @return 生成的文件名
     */
    std::string generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels) {
        // 使用 std::filesystem 处理路径
        std::filesystem::path outputDir;
        
        // 首先尝试使用配置的录音路径
        if (!config_.recordingPath.empty()) {
            outputDir = std::filesystem::path(config_.recordingPath);
        } else {
            // 如果配置路径为空，使用当前工作目录
            char cwd[PATH_MAX];
            if (getcwd(cwd, sizeof(cwd)) == nullptr) {
                std::cerr << "Failed to get current working directory" << std::endl;
                return "";
            }
            outputDir = std::filesystem::path(cwd);
        }
        
        // 确保输出目录存在
        if (!std::filesystem::exists(outputDir)) {
            try {
                std::filesystem::create_directories(outputDir);
            } catch (const std::exception& e) {
                std::cerr << "Failed to create output directory: " << e.what() << std::endl;
                return "";
            }
        }
        
        // 生成文件名
        std::string channelStr = (channels == ChannelCount::MONO) ? "mono" : "stereo";
        std::string extension = (format == "WAV") ? "wav" : "ogg";
        std::string filename = std::string("recording_") + 
                              std::to_string(sampleRate) + "hz_" + 
                              channelStr + "." + extension;
        
        // 组合完整路径
        return (outputDir / filename).string();
    }

    bool startRecording(const std::string& outputFile) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            // 检查当前状态
            if (isRecording_) {
                lastError_ = "Recording is already in progress";
                return false;
            }

            if (!device_ || !device_->isDeviceOpen()) {
                lastError_ = "No audio device available";
                return false;
            }

            // 检查输出文件路径
            std::filesystem::path filePath(outputFile);
            std::filesystem::path parentPath = filePath.parent_path();
            if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
                try {
                    std::filesystem::create_directories(parentPath);
                } catch (const std::exception& e) {
                    lastError_ = std::string("Failed to create output directory: ") + e.what();
                    return false;
                }
            }

            // 检查文件扩展名并设置相应的编码格式
            std::string extension = filePath.extension().string();
            if (extension == ".ogg") {
                std::cout << "[DEBUG] Setting encoding format to OPUS" << std::endl;
                processor_->setEncodingFormat(EncodingFormat::OPUS);
            } else if (extension == ".wav") {
                std::cout << "[DEBUG] Setting encoding format to WAV" << std::endl;
                processor_->setEncodingFormat(EncodingFormat::WAV);
            } else {
                lastError_ = "Unsupported file format: " + extension;
                return false;
            }

            // 清空录音缓冲区
            recordingBuffer_.clear();
            
            // 设置音频回调
            device_->setCallback([this](const void* input, void* output, size_t frameCount) {
                this->audioCallback(input, output, frameCount);
            });
            
            // 开始录音
            std::cout << "[DEBUG] Starting audio stream..." << std::endl;
            if (!device_->startStream()) {
                lastError_ = "Failed to start audio stream: " + device_->getLastError();
                return false;
            }
            
            // 设置输出文件
            currentOutputFile_ = outputFile;
            isRecording_ = true;
            
            std::cout << "[DEBUG] Recording to file: " << currentOutputFile_ << std::endl;
            return true;
        } catch (const std::exception& e) {
            lastError_ = std::string("Exception in startRecording: ") + e.what();
            return false;
        }
    }

    bool stopRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            if (!isRecording_) {
                lastError_ = "No recording in progress";
                return false;
            }

            if (!device_ || !device_->isDeviceOpen()) {
                lastError_ = "No audio device available";
                return false;
            }
            
            // 停止录音
            std::cout << "[DEBUG] Stopping audio stream..." << std::endl;
            if (!device_->stopStream()) {
                lastError_ = "Failed to stop audio stream: " + device_->getLastError();
                return false;
            }
            
            isRecording_ = false;
            
            // 保存录音数据
            if (!recordingBuffer_.empty()) {
                std::cout << "[DEBUG] Saving recording data to file: " << currentOutputFile_ << std::endl;
                
                // 根据文件扩展名选择保存格式
                std::filesystem::path filePath(currentOutputFile_);
                std::string extension = filePath.extension().string();
                
                bool saveSuccess = false;
                if (extension == ".wav") {
                    saveSuccess = writeWavFile(recordingBuffer_.data(), 
                                            recordingBuffer_.size() / static_cast<int>(config_.channels), 
                                            currentOutputFile_);
                } else if (extension == ".ogg") {
                    saveSuccess = writeOpusFile(recordingBuffer_.data(), 
                                              recordingBuffer_.size() / static_cast<int>(config_.channels), 
                                              currentOutputFile_);
                } else {
                    lastError_ = "Unsupported file format: " + extension;
                    return false;
                }

                if (!saveSuccess) {
                    lastError_ = "Failed to save recording data: " + lastError_;
                    return false;
                }
                
                recordingBuffer_.clear();
            }
            
            std::cout << "[DEBUG] Recording stopped, file saved: " << currentOutputFile_ << std::endl;
            return true;
        } catch (const std::exception& e) {
            lastError_ = std::string("Exception in stopRecording: ") + e.what();
            return false;
        }
    }

private:
    bool initialized_;
    bool isRecording_;
    std::shared_ptr<AudioProcessor> processor_;
    std::unique_ptr<AudioThread> thread_;
    std::string currentOutputFile_;
    std::vector<int16_t> recordingBuffer_;
    std::unique_ptr<AudioDevice> device_;
    AudioConfig currentConfig_;
    std::string lastError_;
    std::mutex mutex_;

    // WAV文件头操作
    bool writeWavHeader(const std::string& filename, const WavHeader& header) {
        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            std::cerr << "Failed to open WAV file for writing header: " << filename << std::endl;
            return false;
        }
        file.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        return file.good();
    }

    bool writeWavData(const std::string& filename, const void* data, size_t frames, bool append) {
        if (!data || frames == 0) return false;

        std::ios_base::openmode mode = std::ios::binary;
        if (append) {
            mode |= std::ios::app;
        }

        std::ofstream file(filename, mode);
        if (!file) {
            std::cerr << "Failed to open WAV file for writing data: " << filename << std::endl;
            return false;
        }

        // 如果是追加模式，需要跳过文件头
        if (append) {
            file.seekp(sizeof(WavHeader), std::ios::beg);
        }

        // 写入音频数据
        file.write(static_cast<const char*>(data), frames * sizeof(float) * static_cast<int>(config_.channels));
        return file.good();
    }

    AudioManager::WavHeader generateWavHeader(size_t dataSize) const {
        WavHeader header;
        memcpy(header.riff, "RIFF", 4);
        header.size = 36 + dataSize;
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        header.fmtSize = 16;
        header.format = 1;  // PCM
        header.channels = static_cast<int>(config_.channels);
        header.sampleRate = static_cast<int>(config_.sampleRate);
        header.bitsPerSample = 16;  // 使用16位整数
        header.byteRate = header.sampleRate * header.channels * sizeof(int16_t);
        header.blockAlign = header.channels * sizeof(int16_t);
        memcpy(header.data, "data", 4);
        header.dataSize = dataSize;

        // 打印文件头信息用于调试
        std::cout << "[DEBUG] WAV Header:" << std::endl;
        std::cout << "  - Format: " << header.format << " (1 = PCM)" << std::endl;
        std::cout << "  - Channels: " << header.channels << std::endl;
        std::cout << "  - Sample Rate: " << header.sampleRate << " Hz" << std::endl;
        std::cout << "  - Bits Per Sample: " << header.bitsPerSample << " bits" << std::endl;
        std::cout << "  - Byte Rate: " << header.byteRate << " bytes/sec" << std::endl;
        std::cout << "  - Block Align: " << header.blockAlign << " bytes" << std::endl;
        std::cout << "  - Data Size: " << header.dataSize << " bytes" << std::endl;

        return header;
    }

    // Opus文件头操作
    bool writeOpusHeader(const std::string& filename, const OpusHeader& header) {
        FILE* file = fopen(filename.c_str(), "wb");
        if (!file) {
            std::cerr << "Failed to open Opus file for writing header: " << filename << std::endl;
            return false;
        }

        // 初始化OGG流
        ogg_stream_state os;
        ogg_page og;
        ogg_packet op;

        if (ogg_stream_init(&os, rand()) != 0) {
            fclose(file);
            return false;
        }

        // 写入Opus头
        op.packet = const_cast<unsigned char*>(header.header);
        op.bytes = 19;
        op.b_o_s = 1;
        op.e_o_s = 0;
        op.granulepos = 0;
        op.packetno = 0;

        ogg_stream_packetin(&os, &op);

        // 写入OGG页
        while (ogg_stream_flush(&os, &og)) {
            fwrite(og.header, 1, og.header_len, file);
            fwrite(og.body, 1, og.body_len, file);
        }

        ogg_stream_clear(&os);
        fclose(file);
        return true;
    }

    bool writeOpusData(const std::string& filename, const void* data, size_t frames, bool append) {
        if (!data || frames == 0) return false;

        // 编码音频数据
        std::vector<std::vector<uint8_t>> encodedFrames;
        if (!processor_->encodeOpus(data, frames, encodedFrames)) {
            std::cerr << "Failed to encode audio data" << std::endl;
            return false;
        }

        FILE* file = fopen(filename.c_str(), append ? "ab" : "wb");
        if (!file) {
            std::cerr << "Failed to open Opus file for writing data: " << filename << std::endl;
            return false;
        }

        // 初始化 OGG 流
        ogg_stream_state os;
        ogg_page og;
        ogg_packet op;

        if (ogg_stream_init(&os, rand()) != 0) {
            fclose(file);
            return false;
        }

        // 写入封装后的数据
        int64_t granulepos = 0;
        int packetno = append ? 1 : 0;
        int samplesPerFrame = static_cast<int>(config_.sampleRate) * config_.opusFrameLength / 1000;

        for (size_t i = 0; i < encodedFrames.size(); i++) {
            op.packet = encodedFrames[i].data();
            op.bytes = encodedFrames[i].size();
            op.b_o_s = append ? 0 : (i == 0);
            op.e_o_s = (i == encodedFrames.size() - 1);
            op.granulepos = granulepos;
            op.packetno = packetno++;

            if (ogg_stream_packetin(&os, &op) != 0) {
                std::cerr << "Failed to add packet to OGG stream" << std::endl;
                ogg_stream_clear(&os);
                fclose(file);
                return false;
            }

            granulepos += samplesPerFrame;
            
            // 每写入一帧就刷新一次OGG流
            while (ogg_stream_flush(&os, &og)) {
                if (fwrite(og.header, 1, static_cast<size_t>(og.header_len), file) != static_cast<size_t>(og.header_len) ||
                    fwrite(og.body, 1, static_cast<size_t>(og.body_len), file) != static_cast<size_t>(og.body_len)) {
                    std::cerr << "Failed to write OGG page" << std::endl;
                    ogg_stream_clear(&os);
                    fclose(file);
                    return false;
                }
            }
        }

        ogg_stream_clear(&os);
        fclose(file);
        return true;
    }

    AudioManager::OpusHeader generateOpusHeader(size_t dataSize) const {
        OpusHeader header;
        
        // 设置Opus头
        header.header[0] = 'O';
        header.header[1] = 'p';
        header.header[2] = 'u';
        header.header[3] = 's';
        header.header[4] = 'H';
        header.header[5] = 'e';
        header.header[6] = 'a';
        header.header[7] = 'd';
        header.header[8] = 1;
        header.header[9] = static_cast<int>(config_.channels);
        header.header[10] = 0;
        header.header[11] = 0;
        header.header[12] = (static_cast<int>(config_.sampleRate) >> 0) & 0xFF;
        header.header[13] = (static_cast<int>(config_.sampleRate) >> 8) & 0xFF;
        header.header[14] = (static_cast<int>(config_.sampleRate) >> 16) & 0xFF;
        header.header[15] = (static_cast<int>(config_.sampleRate) >> 24) & 0xFF;
        header.header[16] = 0;
        header.header[17] = 0;
        header.header[18] = 0;

        header.sampleRate = static_cast<int>(config_.sampleRate);
        header.channels = static_cast<int>(config_.channels);
        header.dataSize = dataSize;

        return header;
    }

    void audioCallback(const void* input, void* output, size_t frameCount) {
        if (!initialized_) {
            return;
        }

        if (isRecording_) {
            const int16_t* inputData = static_cast<const int16_t*>(input);
            size_t samples = frameCount * static_cast<int>(config_.channels);
            recordingBuffer_.insert(recordingBuffer_.end(), inputData, inputData + samples);
        }

        processor_->processAudio(input, output, frameCount);
    }
};

// AudioManager构造函数和析构函数
AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

// AudioManager公共接口实现
bool AudioManager::initialize(const AudioConfig& config) { return impl_->initialize(config); }
bool AudioManager::loadConfig(const std::string& configPath) { return impl_->loadConfig(configPath); }
bool AudioManager::saveConfig(const std::string& configPath) { return impl_->saveConfig(configPath); }
std::vector<DeviceInfo> AudioManager::getAvailableDevices() { return impl_->getAvailableDevices(); }
std::shared_ptr<AudioThread> AudioManager::createAudioThread(const AudioConfig& config) { return impl_->createAudioThread(config); }
std::shared_ptr<AudioProcessor> AudioManager::getProcessor() { return impl_->getProcessor(); }
bool AudioManager::updateConfig(const AudioConfig& config) { return impl_->updateConfig(config); }
const AudioConfig& AudioManager::getConfig() const { return impl_->getConfig(); }
void AudioManager::cleanup() { impl_->cleanup(); }

// 文件操作函数实现
bool AudioManager::writeWavFile(const void* input, size_t frames, const std::string& filename) {
    return impl_->writeWavFile(input, frames, filename);
}

bool AudioManager::writeOpusFile(const void* input, size_t frames, const std::string& filename) {
    return impl_->writeOpusFile(input, frames, filename);
}

bool AudioManager::readWavFile(const std::string& filename, std::vector<float>& output, size_t& frames) {
    return impl_->readWavFile(filename, output, frames);
}

std::string AudioManager::generateOutputFilename(const std::string& format, int sampleRate, ChannelCount channels) {
    return impl_->generateOutputFilename(format, sampleRate, channels);
}

bool AudioManager::startRecording(const std::string& outputFile) {
    return impl_->startRecording(outputFile);
}

bool AudioManager::stopRecording() {
    return impl_->stopRecording();
}

} // namespace audio
} // namespace perfx 