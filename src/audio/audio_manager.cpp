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
#include <thread>
#include <chrono>

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
        std::cout << "[DEBUG] ~AudioManager::Impl called" << std::endl;
        cleanup();
    }

    /**
     * @brief 初始化音频管理器
     * @return 初始化是否成功
     */
    bool initialize(const AudioConfig& config) {
        std::cout << "[DEBUG] Entering AudioManager::initialize" << std::endl;
        config_ = config;

        // 检查是否已经初始化
        if (initialized_) {
            std::cout << "[DEBUG] AudioManager::initialize: already initialized, returning true" << std::endl;
            return true;
        }

        // 1. 初始化 PortAudio 库
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            std::cerr << "PortAudio initialization failed: " << Pa_GetErrorText(err) << std::endl;
            return false;
        }

        // 2. 创建并初始化音频设备
        device_ = std::make_unique<AudioDevice>();
        if (!device_->initialize()) {
            std::cerr << "Failed to initialize audio device" << std::endl;
            device_.reset();  // 清理设备资源
            Pa_Terminate();   // 清理 PortAudio
            return false;
        }

        // 3. 打开输入设备
        if (!device_->openInputDevice(config.inputDevice, config)) {
            std::cerr << "Failed to open input device: " << device_->getLastError() << std::endl;
            device_.reset();  // 清理设备资源
            Pa_Terminate();   // 清理 PortAudio
            return false;
        }

        // 4. 验证设备状态
        if (!device_->isDeviceOpen()) {
            std::cerr << "Device is not open after initialization" << std::endl;
            device_.reset();  // 清理设备资源
            Pa_Terminate();   // 清理 PortAudio
            return false;
        }

        // 5. 设置设备回调函数
        device_->setCallback([this](const void* input, void* output, size_t frameCount) {
            this->audioCallback(input, output, frameCount);
        });

        // 6. 初始化音频处理器
        processor_ = std::make_unique<AudioProcessor>();
        if (!processor_->initialize(config)) {
            std::cerr << "Failed to initialize audio processor" << std::endl;
            device_.reset();      // 清理设备资源
            processor_.reset();   // 清理处理器资源
            Pa_Terminate();       // 清理 PortAudio
            return false;
        }

        // 7. 配置编码格式和参数
        processor_->setEncodingFormat(config.encodingFormat);
        if (config.encodingFormat == EncodingFormat::OPUS) {
            // 计算 Opus 帧大小（采样点数）
            int samplesPerFrame = (config.opusFrameLength * static_cast<int>(config.sampleRate)) / 1000;
            std::cout << "[DEBUG] Opus frame size: " << samplesPerFrame << " samples" << std::endl;
            std::cout << "- opusFrameLength:" << config.opusFrameLength << std::endl;
            std::cout << "- sampleRate:" << static_cast<int>(config.sampleRate) << std::endl;
            
            // 检查缓冲区大小是否是帧大小的整数倍
            if (samplesPerFrame % config.framesPerBuffer != 0) {
                std::cerr << "Warning: Opus frame size (" << samplesPerFrame 
                         << ") is not a multiple of buffer size (" << config.framesPerBuffer << ")" << std::endl;
            } else {
                std::cout << "[DEBUG] Buffer size (" << config.framesPerBuffer 
                         << ") is compatible with Opus frame size (" << samplesPerFrame << ")" << std::endl;
            }
            
            processor_->setOpusFrameLength(config.opusFrameLength);
        }


        // 9. 初始化音频线程
        audioStreamThread_ = std::make_shared<AudioThread>();
        if (!audioStreamThread_->initialize(processor_.get())) {
            std::cerr << "Failed to initialize audio thread" << std::endl;
            device_.reset();          // 清理设备资源
            processor_.reset();       // 清理处理器资源
            audioStreamThread_.reset(); // 清理线程资源
            Pa_Terminate();           // 清理 PortAudio
            return false;
        }

        // 10. 标记初始化完成
        initialized_ = true;
        std::cout << "[DEBUG] Exiting AudioManager::initialize, result: success" << std::endl;
        return true;
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
     * @brief 清理资源
     */
    void cleanup() {
        if (isRecording_) {
            stopRecording();
        }
        audioStreamThread_.reset();
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

            // 计算数据大小 - 录音缓冲区始终使用INT16格式
            size_t dataSize = frames * sizeof(int16_t) * static_cast<int>(config_.channels);
            
            std::cout << "[DEBUG] WAV File Write Parameters:" << std::endl;
            std::cout << "  - Input frames: " << frames << std::endl;
            std::cout << "  - Channels: " << static_cast<int>(config_.channels) << std::endl;
            std::cout << "  - Bytes per sample: " << sizeof(int16_t) << std::endl;
            std::cout << "  - Calculated data size: " << dataSize << " bytes" << std::endl;
            std::cout << "  - Expected duration: " << (float)frames / static_cast<int>(config_.sampleRate) << " seconds" << std::endl;

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
                
                // 调试：检查要写入的前几个字节
                if (frames > 0) {
                    const uint8_t* rawBytes = static_cast<const uint8_t*>(input);
                    std::cout << "[DEBUG] Writing first 8 bytes to WAV: ";
                    for (size_t i = 0; i < 8 && i < dataSize; ++i) {
                        printf("%02X ", rawBytes[i]);
                    }
                    std::cout << std::endl;
                    
                    const int16_t* int16Data = static_cast<const int16_t*>(input);
                    std::cout << "[DEBUG] First few samples: ";
                    for (size_t i = 0; i < std::min(static_cast<size_t>(4), frames); ++i) {
                        std::cout << int16Data[i] << " ";
                    }
                    std::cout << std::endl;
                }
                
                file.write(static_cast<const char*>(input), dataSize);
                if (!file.good()) {
                    lastError_ = "Failed to write audio data";
                    return false;
                }
                
                std::cout << "[DEBUG] Successfully wrote " << dataSize << " bytes of audio data" << std::endl;
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
        
        if (isRecording_) {
            AUDIO_LOG("Already recording");
            return false;
        }
        
        if (!device_ || !device_->isDeviceOpen()) {
            AUDIO_LOG("No audio device available");
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
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return false;
            }
        }

        // 检查文件扩展名并设置相应的编码格式
        std::string extension = filePath.extension().string();
        if (extension == ".ogg") {
            AUDIO_LOG("Setting encoding format to OPUS");
            processor_->setEncodingFormat(EncodingFormat::OPUS);
        } else if (extension == ".wav") {
            AUDIO_LOG("Setting encoding format to WAV");
            processor_->setEncodingFormat(EncodingFormat::WAV);
        } else {
            lastError_ = "Unsupported file format: " + extension;
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        // 验证 Opus 编码参数
        if (extension == ".ogg" && (config_.opusBitrate <= 0 || 
                                    config_.opusFrameLength <= 0 || 
                                    config_.opusComplexity < 0 || 
                                    config_.opusComplexity > 10)) {
            lastError_ = "Invalid Opus encoding parameters";
            return false;
        }

        // 清空录音缓冲区
        recordingBuffer_.clear();
        
        // 开始录音
        AUDIO_LOG("Starting audio stream...");
        
        // 创建并初始化音频线程
        audioStreamThread_ = std::make_shared<AudioThread>();
        if (!audioStreamThread_->initialize(processor_.get())) {
            lastError_ = "Failed to initialize audio thread";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        // 设置音频处理器，确保 AudioThread 使用相同的处理器实例
        audioStreamThread_->setProcessor(processor_);
        std::cout << "[DEBUG] AudioManager set processor to AudioThread: " << static_cast<void*>(processor_.get()) << std::endl;

        // 设置输入设备
        if (!audioStreamThread_->setInputDevice(device_->getCurrentDevice())) {
            lastError_ = "Failed to set input device";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        // 设置音频回调
        audioStreamThread_->setInputCallback([this](const void* input, void* output, size_t frameCount) {
            this->audioCallback(input, output, frameCount);
        });

        // 启动音频流
        audioStreamThread_->start();

        // 统一启动延迟：让音频流稳定后再开始录音
        std::cout << "[DEBUG] Waiting for audio stream to stabilize..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));  // 200ms延迟
        std::cout << "[DEBUG] Audio stream stabilization complete" << std::endl;

        // 开始录音
        if (!audioStreamThread_->startRecording()) {
            lastError_ = "Failed to start recording";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            std::cerr << "[DEBUG] audioStreamThread_->isRunning(): " << audioStreamThread_->isRunning() << std::endl;
            return false;
        }

        // 设置输出文件
        currentOutputFile_ = outputFile;
        isRecording_ = true;
        
        AUDIO_LOG("Starting recording to file: " + outputFile);
        return true;
    }

    bool stopRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!isRecording_) {
            AUDIO_LOG("Not recording");
            return false;
        }
        
        if (!device_ || !device_->isDeviceOpen()) {
            AUDIO_LOG("No audio device available");
            return false;
        }
        
        // 停止录音
        AUDIO_LOG("Stopping audio stream...");
        if (audioStreamThread_) {
            audioStreamThread_->stop();
        }
        
        isRecording_ = false;
        
        // 保存录音数据
        if (!recordingBuffer_.empty()) {
            AUDIO_LOG("Saving recording data to file: " + currentOutputFile_);
            
            // 调试：检查录音缓冲区的内容
            std::cout << "[DEBUG] Recording buffer analysis:" << std::endl;
            std::cout << "  - Buffer size: " << recordingBuffer_.size() << " samples" << std::endl;
            std::cout << "  - Channels: " << static_cast<int>(config_.channels) << std::endl;
            std::cout << "  - Frame count: " << recordingBuffer_.size() / static_cast<int>(config_.channels) << std::endl;
            
            // 检查前几个样本
            std::cout << "  - First 8 samples: ";
            for (size_t i = 0; i < std::min(static_cast<size_t>(8), recordingBuffer_.size()); ++i) {
                std::cout << recordingBuffer_[i] << " ";
            }
            std::cout << std::endl;
            
            // 统计非零样本
            size_t nonZeroSamples = 0;
            int16_t maxSample = 0, minSample = 0;
            for (size_t i = 0; i < recordingBuffer_.size(); ++i) {
                if (recordingBuffer_[i] != 0) {
                    nonZeroSamples++;
                    maxSample = std::max(maxSample, recordingBuffer_[i]);
                    minSample = std::min(minSample, recordingBuffer_[i]);
                }
            }
            std::cout << "  - Non-zero samples: " << nonZeroSamples << " / " << recordingBuffer_.size() << std::endl;
            std::cout << "  - Sample range: " << minSample << " to " << maxSample << std::endl;
            
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
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return false;
            }
            
            // 验证文件是否成功创建
            if (!std::filesystem::exists(currentOutputFile_)) {
                lastError_ = "Output file was not created: " + currentOutputFile_;
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return false;
            }
            
            AUDIO_LOG("Recording stopped, file saved: " + currentOutputFile_);
            // AUDIO_LOG_VAR(std::filesystem::file_size(currentOutputFile_)); // 移除未声明变量
            
            recordingBuffer_.clear();
        } else {
            AUDIO_LOG("Recording buffer is empty, no data to save");
        }
        
        return true;
    }


private:
    bool initialized_;
    bool isRecording_;
    std::shared_ptr<AudioProcessor> processor_;
    std::shared_ptr<AudioThread> audioStreamThread_;
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

        // 写入音频数据（INT16格式）
        file.write(static_cast<const char*>(data), frames * sizeof(int16_t) * static_cast<int>(config_.channels));
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
        
        // 注意：无论输入格式是什么，录音缓冲区始终存储为INT16格式
        // 在audioCallback中，FLOAT32数据会被转换为INT16并存储到recordingBuffer_
        // 因此WAV文件头应该始终使用16位配置
        header.bitsPerSample = 16;  // 录音缓冲区使用INT16格式
        header.byteRate = header.sampleRate * header.channels * sizeof(int16_t);
        header.blockAlign = header.channels * sizeof(int16_t);
        
        memcpy(header.data, "data", 4);
        header.dataSize = dataSize;

        // 打印文件头信息用于调试
        std::cout << "[DEBUG] WAV Header Generated:" << std::endl;
        std::cout << "  - Format: " << header.format << " (1 = PCM)" << std::endl;
        std::cout << "  - Channels: " << header.channels << std::endl;
        std::cout << "  - Sample Rate: " << header.sampleRate << " Hz" << std::endl;
        std::cout << "  - Bits Per Sample: " << header.bitsPerSample << " bits (buffer format)" << std::endl;
        std::cout << "  - Byte Rate: " << header.byteRate << " bytes/sec" << std::endl;
        std::cout << "  - Block Align: " << header.blockAlign << " bytes" << std::endl;
        std::cout << "  - Data Size: " << header.dataSize << " bytes" << std::endl;
        std::cout << "  - Expected duration: " << (float)dataSize / header.byteRate << " seconds" << std::endl;
        std::cout << "  - Input device format: " << static_cast<int>(config_.format) << " (INT16=0, FLOAT32=3)" << std::endl;

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
            std::cerr << "[ERROR] Audio callback called before initialization" << std::endl;
            return;
        }

        if (isRecording_) {            
            // 将数据添加到录音缓冲区
            size_t samplesPerFrame = static_cast<int>(config_.channels);
            size_t totalSamples = frameCount * samplesPerFrame;
            
            // 简化调试输出，每100帧输出一次状态
            static int frameCounter = 0;
            frameCounter++;
            if (frameCounter % 100 == 0) {
                std::cout << "[DEBUG] Recording: frame " << frameCounter 
                         << ", config_format=" << static_cast<int>(config_.format)
                         << ", samples=" << totalSamples << std::endl;
                
                // 检查实际数据内容
                if (frameCount > 0 && input) {
                    const uint8_t* rawBytes = static_cast<const uint8_t*>(input);
                    std::cout << "[DEBUG] Raw data check: ";
                    for (size_t i = 0; i < 8 && i < frameCount * 4; ++i) {
                        printf("%02X ", rawBytes[i]);
                    }
                    std::cout << std::endl;
                    
                    // 尝试按两种格式解释数据
                    const int16_t* asInt16 = static_cast<const int16_t*>(input);
                    const float* asFloat32 = static_cast<const float*>(input);
                    std::cout << "[DEBUG] As INT16: " << asInt16[0] << " " << asInt16[1] << std::endl;
                    std::cout << "[DEBUG] As FLOAT32: " << asFloat32[0] << " " << asFloat32[1] << std::endl;
                }
            }
            
            // 根据设备的实际输出格式处理数据
            // 从调试信息看：设备报告Sample format: 1 (paInt16)，但可能实际输出FLOAT32
            // 我们需要根据设备信息和数据特征来正确判断
            
            // 检查数据是否为空
            if (!input) {
                std::cerr << "[ERROR] Input buffer is null!" << std::endl;
                return;
            }
            
            // 获取设备的实际格式信息
            bool deviceReportsFloat32 = false;
            if (device_ && device_->isDeviceOpen()) {
                AudioConfig deviceConfig = device_->getCurrentConfig();
                deviceReportsFloat32 = (deviceConfig.format == SampleFormat::FLOAT32);
                
                if (frameCounter % 100 == 0) {
                    std::cout << "[DEBUG] Device config format: " << static_cast<int>(deviceConfig.format) 
                             << " (INT16=0, FLOAT32=3)" << std::endl;
                }
            }
            
            // 改进的格式检测逻辑
            bool treatAsFloat32 = false;
            if (frameCount > 0) {
                const float* floatData = static_cast<const float*>(input);
                const int16_t* int16Data = static_cast<const int16_t*>(input);
                
                // 检查FLOAT32数据的有效性和合理性
                bool validFloatRange = true;
                bool hasReasonableFloatValues = false;
                float maxAbs = 0.0f;
                
                for (size_t i = 0; i < std::min(frameCount, static_cast<size_t>(10)); ++i) {
                    float sample = floatData[i];
                    if (!std::isfinite(sample) || sample < -100.0f || sample > 100.0f) {
                        validFloatRange = false;
                        break;
                    }
                    maxAbs = std::max(maxAbs, std::abs(sample));
                    // 如果浮点值在合理的音频范围内（通常-1到1，但允许稍大）
                    if (std::abs(sample) > 0.001f && std::abs(sample) < 10.0f) {
                        hasReasonableFloatValues = true;
                    }
                }
                
                // 检查INT16数据的特征
                bool hasLargeInt16Values = false;
                for (size_t i = 0; i < std::min(frameCount, static_cast<size_t>(10)); ++i) {
                    int16_t sample = int16Data[i];
                    if (std::abs(sample) > 1000) {  // 较大的INT16值
                        hasLargeInt16Values = true;
                    }
                }
                
                // 决策逻辑：优先考虑FLOAT32，特别是当：
                // 1. 设备报告FLOAT32格式，或
                // 2. 浮点数据在合理范围内且有意义的值，或  
                // 3. INT16解释产生的值看起来不像音频数据
                if (deviceReportsFloat32 || 
                    (validFloatRange && hasReasonableFloatValues && maxAbs < 5.0f) ||
                    (!hasLargeInt16Values && hasReasonableFloatValues)) {
                    treatAsFloat32 = true;
                }
                
                if (frameCounter % 100 == 0) {
                    std::cout << "[DEBUG] Format analysis:" << std::endl;
                    std::cout << "  - deviceReportsFloat32: " << deviceReportsFloat32 << std::endl;
                    std::cout << "  - validFloatRange: " << validFloatRange << std::endl;
                    std::cout << "  - hasReasonableFloatValues: " << hasReasonableFloatValues << std::endl;
                    std::cout << "  - maxAbs: " << maxAbs << std::endl;
                    std::cout << "  - hasLargeInt16Values: " << hasLargeInt16Values << std::endl;
                    std::cout << "  - DECISION: treatAsFloat32 = " << treatAsFloat32 << std::endl;
                }
            }
            
            if (treatAsFloat32) {
                // 按FLOAT32处理
                const float* floatData = static_cast<const float*>(input);
                
                // 添加详细的转换调试信息
                size_t bufferSizeBefore = recordingBuffer_.size();
                
                // 分析音频数据的实际范围，而不是强制限制到-1.0~1.0
                float minSample = 0.0f, maxSample = 0.0f;
                for (size_t i = 0; i < totalSamples; ++i) {
                    float sample = floatData[i];
                    minSample = std::min(minSample, sample);
                    maxSample = std::max(maxSample, sample);
                }
                
                // 计算合适的缩放因子，避免削波
                float maxAbs = std::max(std::abs(minSample), std::abs(maxSample));
                float scaleFactor = 1.0f;
                if (maxAbs > 1.0f) {
                    scaleFactor = 1.0f / maxAbs;  // 缩放以防止削波
                }
                
                for (size_t i = 0; i < totalSamples; ++i) {
                    // 使用动态缩放而不是硬限制
                    float sample = floatData[i] * scaleFactor;
                    int16_t int16Sample = static_cast<int16_t>(sample * 32767);
                    recordingBuffer_.push_back(int16Sample);
                }
                
                if (frameCounter % 100 == 0) {
                    std::cout << "[DEBUG] FLOAT32 conversion details:" << std::endl;
                    std::cout << "  - Buffer size before: " << bufferSizeBefore << std::endl;
                    std::cout << "  - Buffer size after: " << recordingBuffer_.size() << std::endl;
                    std::cout << "  - Added samples: " << totalSamples << std::endl;
                    std::cout << "  - Input range: " << minSample << " to " << maxSample << std::endl;
                    std::cout << "  - Max absolute: " << maxAbs << std::endl;
                    std::cout << "  - Scale factor: " << scaleFactor << std::endl;
                    
                    // 显示前几个转换结果
                    if (totalSamples > 0) {
                        size_t startIndex = bufferSizeBefore;
                        std::cout << "  - First few conversions:" << std::endl;
                        for (size_t i = 0; i < std::min(totalSamples, static_cast<size_t>(4)); ++i) {
                            float originalFloat = floatData[i];
                            float scaledFloat = originalFloat * scaleFactor;
                            int16_t convertedInt16 = static_cast<int16_t>(scaledFloat * 32767);
                            std::cout << "    " << i << ": " << originalFloat << " -> " 
                                     << scaledFloat << " -> " << convertedInt16;
                            if (startIndex + i < recordingBuffer_.size()) {
                                std::cout << " (stored: " << recordingBuffer_[startIndex + i] << ")";
                            }
                            std::cout << std::endl;
                        }
                    }
                    
                    std::cout << "[DEBUG] Processed as FLOAT32 data" << std::endl;
                }
            } else {
                // 按INT16处理
                const int16_t* inputData = static_cast<const int16_t*>(input);
                recordingBuffer_.insert(recordingBuffer_.end(), inputData, inputData + totalSamples);
                if (frameCounter % 100 == 0) {
                    std::cout << "[DEBUG] Processed as INT16 data" << std::endl;
                }
            }
        }

        // 处理输出（如果需要）
        if (output) {
            // 这里可以添加输出处理逻辑
        }
    }

};

// AudioManager构造函数和析构函数
AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {}
AudioManager::~AudioManager() = default;

// AudioManager公共接口实现
bool AudioManager::initialize(const AudioConfig& config) { return impl_->initialize(config); }
std::vector<DeviceInfo> AudioManager::getAvailableDevices() { return impl_->getAvailableDevices(); }
std::shared_ptr<AudioThread> AudioManager::createAudioThread(const AudioConfig& config) { return impl_->createAudioThread(config); }
std::shared_ptr<AudioProcessor> AudioManager::getProcessor() { return impl_->getProcessor(); }
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


bool AudioManager::loadAudioConfig(AudioConfig& inputConfig, OutputSettings& outputSettings, const std::string& configPath) {
    try {
        if (!std::filesystem::exists(configPath)) {
            std::cout << "Config file not found: " << configPath << std::endl;
            return false;
        }
        
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << configPath << std::endl;
            return false;
        }
        
        nlohmann::json config;
        file >> config;
        file.close();
        
        // 加载基础音频参数
        if (config.contains("audio")) {
            auto& audio = config["audio"];
            inputConfig.sampleRate = static_cast<SampleRate>(audio["sampleRate"]);
            inputConfig.channels = static_cast<ChannelCount>(audio["channels"]);
            
            // 处理音频格式字符串
            std::string formatStr = audio["format"];
            if (formatStr == "FLOAT32") {
                inputConfig.format = SampleFormat::FLOAT32;
            } else if (formatStr == "INT16") {
                inputConfig.format = SampleFormat::INT16;
            } else if (formatStr == "INT24") {
                inputConfig.format = SampleFormat::INT24;
            } else {
                inputConfig.format = SampleFormat::INT16; // 默认使用INT16
            }
            
            inputConfig.framesPerBuffer = audio["framesPerBuffer"];
        }
        
        // 加载设备信息
        if (config.contains("device")) {
            auto& device = config["device"];
            inputConfig.inputDevice.name = device["name"];
            inputConfig.inputDevice.index = device["index"];
            
            // 处理设备类型字符串
            std::string typeStr = device["type"];
            if (typeStr == "INPUT") {
                inputConfig.inputDevice.type = DeviceType::INPUT;
            } else if (typeStr == "OUTPUT") {
                inputConfig.inputDevice.type = DeviceType::OUTPUT;
            } else if (typeStr == "BOTH") {
                inputConfig.inputDevice.type = DeviceType::BOTH;
            } else {
                inputConfig.inputDevice.type = DeviceType::INPUT; // 默认值
            }
            
            inputConfig.inputDevice.maxInputChannels = device["maxInputChannels"];
            inputConfig.inputDevice.maxOutputChannels = device["maxOutputChannels"];
            inputConfig.inputDevice.defaultSampleRate = device["defaultSampleRate"];
            inputConfig.inputDevice.defaultLatency = device["defaultLatency"];
        }
        
        // 加载编码参数
        if (config.contains("encoding")) {
            auto& encoding = config["encoding"];
            
            // 处理编码格式字符串
            std::string formatStr = encoding["format"];
            if (formatStr == "WAV") {
                outputSettings.format = EncodingFormat::WAV;
            } else if (formatStr == "OPUS") {
                outputSettings.format = EncodingFormat::OPUS;
            } else {
                outputSettings.format = EncodingFormat::WAV; // 默认值
            }
            
            outputSettings.opusFrameLength = encoding["opusFrameLength"];
            outputSettings.opusBitrate = encoding["opusBitrate"];
            outputSettings.opusComplexity = encoding["opusComplexity"];
            
            // 处理OPUS应用类型字符串
            std::string appStr = encoding["opusApplication"];
            // 查找匹配的应用类型
            bool found = false;
            for (const auto& opt : OPUS_APPLICATION_OPTIONS) {
                if (appStr.find(opt.description) != std::string::npos) {
                    outputSettings.opusApplication = opt.opusApplication;
                    found = true;
                    break;
                }
            }
            if (!found) {
                outputSettings.opusApplication = 2048; // 默认VOIP
            }
        }
        
        // 加载输出参数
        if (config.contains("output")) {
            auto& output = config["output"];
            outputSettings.outputFile = output["outputFile"];
        }
        
        
        std::cout << "✓ Configuration loaded from: " << configPath << std::endl;
        
        // 显示加载的配置摘要
        if (config.contains("metadata")) {
            auto& metadata = config["metadata"];
            std::cout << "  - Version: " << metadata.value("version", "unknown") << std::endl;
            std::cout << "  - Description: " << metadata.value("description", "No description") << std::endl;
            if (metadata.contains("timestamp")) {
                std::time_t timestamp = metadata["timestamp"];
                std::cout << "  - Last saved: " << std::ctime(&timestamp);
            }
        }
        
        // 显示实际加载的采样率（可能被高级用户修改过）
        if (config.contains("audio") && static_cast<int>(inputConfig.sampleRate) != 48000) {
            std::cout << "  ℹ️  Custom sample rate detected: " << static_cast<int>(inputConfig.sampleRate) << " Hz" << std::endl;
            std::cout << "     System will auto-resample for module compatibility as needed." << std::endl;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading audio config: " << e.what() << std::endl;
        return false;
    }
}

bool AudioManager::saveAudioConfig(const AudioConfig& inputConfig, const OutputSettings& outputSettings, const std::string& configPath) {
    try {
        nlohmann::json config;
        
        // 基础音频参数 (Audio Parameters)
        config["audio"]["sampleRate"] = 48000;  // 固定为48K
        config["audio"]["sampleRate_description"] = "采样率 (Hz): 默认48000Hz，高级用户可修改为8000/16000/32000/44100/48000";
        
        config["audio"]["channels"] = static_cast<int>(inputConfig.channels);
        config["audio"]["channels_description"] = "声道数: 1=单声道(MONO), 2=立体声(STEREO)";
        
        // 使用可读的字符串表示格式
        std::string formatStr = (inputConfig.format == SampleFormat::FLOAT32) ? "FLOAT32" : 
                               (inputConfig.format == SampleFormat::INT16) ? "INT16" :
                               (inputConfig.format == SampleFormat::INT24) ? "INT24" : "UNKNOWN";
        config["audio"]["format"] = formatStr;
        config["audio"]["format_description"] = "音频格式: INT16=16位整数(默认), INT24=24位整数, FLOAT32=32位浮点";
        config["audio"]["format_note"] = "高级用户可通过修改此字段更改采样格式，支持INT16/INT24/FLOAT32";
        
        config["audio"]["framesPerBuffer"] = inputConfig.framesPerBuffer;
        config["audio"]["framesPerBuffer_description"] = "缓冲区帧数: 通常为 128/256/512";
        
        // 设备信息 (Device Information)
        config["device"]["name"] = inputConfig.inputDevice.name;
        config["device"]["index"] = inputConfig.inputDevice.index;
        config["device"]["index_description"] = "设备索引: 系统分配的设备编号";
        
        std::string deviceTypeStr = (inputConfig.inputDevice.type == DeviceType::INPUT) ? "INPUT" :
                                   (inputConfig.inputDevice.type == DeviceType::OUTPUT) ? "OUTPUT" :
                                   (inputConfig.inputDevice.type == DeviceType::BOTH) ? "BOTH" : "UNKNOWN";
        config["device"]["type"] = deviceTypeStr;
        config["device"]["type_description"] = "设备类型: INPUT=输入, OUTPUT=输出, BOTH=双向";
        
        config["device"]["maxInputChannels"] = inputConfig.inputDevice.maxInputChannels;
        config["device"]["maxOutputChannels"] = inputConfig.inputDevice.maxOutputChannels;
        config["device"]["defaultSampleRate"] = inputConfig.inputDevice.defaultSampleRate;
        config["device"]["defaultLatency"] = inputConfig.inputDevice.defaultLatency;
        config["device"]["capabilities_description"] = "设备能力: 最大输入/输出通道数, 默认采样率和延迟";
        
        // 编码参数 (Encoding Parameters)
        std::string encodingFormatStr = (outputSettings.format == EncodingFormat::WAV) ? "WAV" : "OPUS";
        config["encoding"]["format"] = encodingFormatStr;
        config["encoding"]["format_description"] = "编码格式: WAV=无损, OPUS=有损压缩";
        
        config["encoding"]["opusFrameLength"] = outputSettings.opusFrameLength;
        config["encoding"]["opusBitrate"] = outputSettings.opusBitrate;
        config["encoding"]["opusComplexity"] = outputSettings.opusComplexity;
        
        // 查找OPUS应用类型的描述
        std::string opusAppStr = "UNKNOWN";
        std::string opusAppDesc = "Unknown application type";
        for (const auto& opt : OPUS_APPLICATION_OPTIONS) {
            if (opt.opusApplication == outputSettings.opusApplication) {
                opusAppStr = std::string(opt.description);
                break;
            }
        }
        config["encoding"]["opusApplication"] = opusAppStr;
        config["encoding"]["opusApplication_description"] = "OPUS应用优化: VOIP=语音通话, Audio=音乐流媒体, Restricted=低延迟";
        
        // 输出参数 (Output Parameters)
        config["output"]["outputFile"] = outputSettings.outputFile;
        config["output"]["outputFile_description"] = "输出文件路径: 录音保存的完整文件路径";
        config["output"]["resampling_note"] = "重采样处理: 系统会自动处理模块间采样率转换 (如rnnoise需要48K，OPUS支持多种采样率)";
        
        
        // 元数据 (Metadata)
        config["metadata"]["version"] = "1.2";
        config["metadata"]["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        config["metadata"]["description"] = "PerfX Audio Recording Configuration";
        config["metadata"]["note"] = "此配置文件包含所有音频录制参数，可直接编辑或通过程序重新生成";
        config["metadata"]["usage_note"] = "普通用户: 程序自动使用48K采样率; 高级用户: 可编辑sampleRate字段自定义采样率";
        config["metadata"]["changes_v1.1"] = "移除手动resampling配置，系统现在自动处理采样率转换";
        config["metadata"]["changes_v1.2"] = "固定48K采样率简化用户操作，高级用户可通过JSON配置自定义";
        
        // 写入文件
        std::ofstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file for writing: " << configPath << std::endl;
            return false;
        }
        
        file << config.dump(4); // 美化格式，缩进4个空格
        file.close();
        
        std::cout << "✓ Configuration saved to: " << configPath << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving audio config: " << e.what() << std::endl;
        return false;
    }
}

} // namespace audio
} // namespace perfx 