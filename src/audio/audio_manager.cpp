/**
 * @file audio_manager.cpp
 * @brief 音频管理器实现文件，负责音频设备、处理器和线程的管理
 */

#include "audio/audio_manager.h"
#include "audio/audio_types.h"
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
#include <functional>

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
    Impl() : initialized_(false) {}

    /**
     * @brief 析构函数
     * 清理资源
     */
    ~Impl() {
        std::cout << "[AUDIO-THREAD] Destroying AudioManager..." << std::endl;
        
        try {
            // 停止录音
            if (recordingInfo_.state != RecordingState::IDLE) {
                std::cout << "[AUDIO-THREAD] Stopping active recording..." << std::endl;
                stopStreamRecording();
            }
            
            // 停止音频线程
            if (audioThread_) {
                std::cout << "[AUDIO-THREAD] Stopping audio thread..." << std::endl;
                audioThread_->stop();
                std::cout << "[AUDIO-THREAD] Audio thread stopped" << std::endl;
            }
            
            // 清理资源
            if (device_) {
                std::cout << "[AUDIO-THREAD] Cleaning up audio device..." << std::endl;
                device_.reset();
                std::cout << "[AUDIO-THREAD] Audio device cleaned up" << std::endl;
            }
            
            std::cout << "[AUDIO-THREAD] AudioManager destroyed successfully" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[AUDIO-THREAD][ERROR] Exception in AudioManager destructor: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[AUDIO-THREAD][ERROR] Unknown exception in AudioManager destructor" << std::endl;
        }
    }

    /**
     * @brief 初始化音频管理器
     * @return 初始化是否成功
     */
    bool initialize(const AudioConfig& config) {
        std::cout << "[AUDIO-THREAD] Initializing AudioManager..." << std::endl;
        
        try {
            config_ = config;

            // **关键修复：如果已经初始化，先清理之前的资源**
            if (initialized_) {
                std::cout << "[AUDIO-THREAD] AudioManager::initialize: already initialized, cleaning up first..." << std::endl;
                cleanup();
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
                std::cerr << "[AUDIO-THREAD][ERROR] Failed to initialize audio device" << std::endl;
                throw std::runtime_error("Failed to initialize audio device");
            }

            // 3. 根据配置决定是否打开输入设备
            if (config.inputDevice.index >= 0) {
                std::cout << "[AUDIO-THREAD] Opening input device with index: " << config.inputDevice.index << std::endl;
                if (!device_->openInputDevice(config.inputDevice, config)) {
                    std::cerr << "Failed to open input device: " << device_->getLastError() << std::endl;
                    device_.reset();  // 清理设备资源
                    Pa_Terminate();   // 清理 PortAudio
                    return false;
                }

                // 设置设备回调函数
                device_->setCallback([this](const void* input, void* output, size_t frameCount) {
                    this->audioCallback(input, output, frameCount);
                });
            } else {
                std::cout << "[AUDIO-THREAD] No input device specified, operating in file/buffer mode" << std::endl;
            }

            // 4. 初始化音频处理器
            processor_ = std::make_unique<AudioProcessor>();
            if (!processor_->initialize(config)) {
                std::cerr << "Failed to initialize audio processor" << std::endl;
                device_.reset();      // 清理设备资源
                processor_.reset();   // 清理处理器资源
                Pa_Terminate();       // 清理 PortAudio
                return false;
            }

            // 5. 配置编码格式和参数
            processor_->setEncodingFormat(config.encodingFormat);
            if (config.encodingFormat == EncodingFormat::OPUS) {
                // 计算 Opus 帧大小（采样点数）
                int samplesPerFrame = (config.opusFrameLength * static_cast<int>(config.sampleRate)) / 1000;
                std::cout << "[AUDIO-THREAD] Opus frame size: " << samplesPerFrame << " samples" << std::endl;
                std::cout << "- opusFrameLength:" << config.opusFrameLength << std::endl;
                std::cout << "- sampleRate:" << static_cast<int>(config.sampleRate) << std::endl;
                
                // 检查缓冲区大小是否是帧大小的整数倍
                if (samplesPerFrame % config.framesPerBuffer != 0) {
                    std::cerr << "Warning: Opus frame size (" << samplesPerFrame 
                             << ") is not a multiple of buffer size (" << config.framesPerBuffer << ")" << std::endl;
                } else {
                    std::cout << "[AUDIO-THREAD] Buffer size (" << config.framesPerBuffer 
                             << ") is compatible with Opus frame size (" << samplesPerFrame << ")" << std::endl;
                }
                
                processor_->setOpusFrameLength(config.opusFrameLength);
            }

            // 6. 初始化音频线程（仅在设备模式下需要）
            if (config.inputDevice.index >= 0) {
                audioThread_ = std::make_unique<AudioThread>();
                if (!audioThread_->initialize(processor_.get())) {
                    std::cerr << "[AUDIO-THREAD][ERROR] Failed to initialize audio thread" << std::endl;
                    throw std::runtime_error("Failed to initialize audio thread");
                }
            }

            // 7. 标记初始化完成
            initialized_ = true;
            std::cout << "[AUDIO-THREAD] AudioManager initialization completed successfully" << std::endl;

            if (device_) {
                device_->setErrorCallback([this](const std::string& errorMsg) {
                    if (onError_) onError_(errorMsg);
                });
            }
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[AUDIO-THREAD][ERROR] Exception in AudioManager::initialize: " << e.what() << std::endl;
            return false;
        }
    }

    // ============================================================================
    // 流式录音功能实现
    // ============================================================================

    bool startStreamRecording(const std::string& outputFile) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state != RecordingState::IDLE) {
            lastError_ = "Already recording or in invalid state";
            return false;
        }
        
        // 启动音频流
        if (!startAudioStream()) {
            return false;
        }
        
        // 如果有输出文件，则初始化WAV文件
        if (!outputFile.empty()) {
            if (!initializeWavFile(outputFile)) {
                // 如果初始化WAV文件失败，停止音频流
                if (audioThread_) {
                    audioThread_->stop();
                }
                return false;
            }
            recordingInfo_.state = RecordingState::RECORDING;
        } else {
            recordingInfo_.state = RecordingState::PREVIEWING;
        }
        
        // 更新录音信息
        recordingInfo_.outputFile = outputFile;
        recordingInfo_.startTime = std::chrono::steady_clock::now();
        recordingInfo_.recordedBytes = 0;
        recordingInfo_.recordedFrames = 0;
        recordingInfo_.totalPausedTime = 0.0;
        
        std::cout << "[AUDIO-THREAD] Stream recording started: " << (outputFile.empty() ? "waveform only" : outputFile) << std::endl;
        return true;
    }
    
    bool startAudioStreamOnly() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state != RecordingState::IDLE) {
            lastError_ = "Already recording or in invalid state";
            return false;
        }
        
        // 仅启动音频流，不保存文件
        if (!startAudioStream()) {
            return false;
        }
        
        // 更新录音信息为波形显示模式
        recordingInfo_.state = RecordingState::RECORDING;
        recordingInfo_.outputFile = "";  // 空字符串表示不保存文件
        recordingInfo_.startTime = std::chrono::steady_clock::now();
        recordingInfo_.recordedBytes = 0;
        recordingInfo_.recordedFrames = 0;
        recordingInfo_.totalPausedTime = 0.0;
        
        std::cout << "[AUDIO-THREAD] Audio stream started for waveform display only" << std::endl;
        return true;
    }
    
    bool pauseStreamRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state != RecordingState::RECORDING) {
            return false;
        }
        
        // 只改变状态，不停止音频流
        recordingInfo_.state = RecordingState::PAUSED;
        recordingInfo_.pauseTime = std::chrono::steady_clock::now();
        
        std::cout << "[AUDIO-THREAD] Stream recording paused (audio stream kept running)" << std::endl;
        return true;
    }
    
    bool resumeStreamRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state != RecordingState::PAUSED) {
            return false;
        }
        
        // 计算暂停时间
        auto now = std::chrono::steady_clock::now();
        auto pauseDuration = std::chrono::duration<double>(now - recordingInfo_.pauseTime);
        recordingInfo_.totalPausedTime += pauseDuration.count();
        
        recordingInfo_.state = RecordingState::RECORDING;
        
        std::cout << "[AUDIO-THREAD] Stream recording resumed" << std::endl;
        return true;
    }
    
    bool stopStreamRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state == RecordingState::IDLE) {
            return true; // 已经是空闲状态
        }
        
        // 标记为正在停止
        recordingInfo_.state = RecordingState::STOPPING;
        
        // 如果有输出文件，完成WAV文件
        if (!recordingInfo_.outputFile.empty()) {
            finalizeWavFile();
            std::cout << "[AUDIO-THREAD] Recording completed: " << recordingInfo_.outputFile << std::endl;
        }
        
        // 停止音频流
        if (audioThread_) {
            audioThread_->stop();
        }
        
        // 重置录音信息
        recordingInfo_.state = RecordingState::IDLE;
        recordingInfo_.outputFile.clear();
        recordingInfo_.recordedBytes = 0;
        recordingInfo_.recordedFrames = 0;
        recordingInfo_.totalPausedTime = 0.0;
        
        std::cout << "[AUDIO-THREAD] Recording stopped" << std::endl;
        return true;
    }
    
    RecordingState getRecordingState() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recordingInfo_.state;
    }
    
    RecordingInfo getRecordingInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recordingInfo_;
    }
    
    double getRecordingDuration() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state == RecordingState::IDLE) {
            return 0.0;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto totalDuration = std::chrono::duration<double>(now - recordingInfo_.startTime);
        
        if (recordingInfo_.state == RecordingState::PAUSED) {
            auto pauseDuration = std::chrono::duration<double>(now - recordingInfo_.pauseTime);
            return totalDuration.count() - recordingInfo_.totalPausedTime - pauseDuration.count();
        }
        
        return totalDuration.count() - recordingInfo_.totalPausedTime;
    }
    
    size_t getRecordedBytes() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return recordingInfo_.recordedBytes;
    }
    
    QVector<float> getLatestWaveformData() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return latestWaveformData_;
    }

    // 将一些方法移到public部分
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

    std::shared_ptr<AudioThread> createAudioThread([[maybe_unused]] const AudioConfig& config) {
        if (!initialized_) {
            std::cerr << "AudioManager not initialized" << std::endl;
            return nullptr;
        }
        
        auto thread = std::make_shared<AudioThread>();
        if (!thread->initialize(processor_.get())) {
            std::cerr << "Failed to initialize audio thread" << std::endl;
            return nullptr;
        }
        
        return thread;
    }

    std::shared_ptr<AudioProcessor> getProcessor() {
        return processor_;
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!initialized_) {
            std::cout << "[AUDIO-THREAD] AudioManager::cleanup: already cleaned up, skipping" << std::endl;
            return;
        }
        
        std::cout << "[AUDIO-THREAD] AudioManager::cleanup: starting cleanup..." << std::endl;
        
        // 1. 停止音频流
        if (device_ && device_->isStreamActive()) {
            std::cout << "[AUDIO-THREAD] Stopping active audio stream..." << std::endl;
            device_->stopStream();
            std::cout << "[AUDIO-THREAD] Audio stream stopped" << std::endl;
        }
        
        // 2. 停止音频线程 - 改进停止逻辑
        if (audioThread_) {
            std::cout << "[AUDIO-THREAD] Stopping audio thread..." << std::endl;
            
            // 先停止线程
            audioThread_->stop();
            
            // 等待线程结束，添加超时机制
            auto startTime = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(5); // 增加到5秒超时
            
            while (audioThread_->isRunning()) {
                auto now = std::chrono::steady_clock::now();
                if (now - startTime > timeout) {
                    std::cout << "[WARNING] Audio thread stop timeout after 5 seconds, forcing cleanup" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 减少等待间隔
            }
            
            // 强制重置线程
            audioThread_.reset();
            std::cout << "[AUDIO-THREAD] Audio thread cleaned up" << std::endl;
        }
        
        // 3. 关闭音频设备
        if (device_) {
            std::cout << "[AUDIO-THREAD] Closing audio device..." << std::endl;
            device_->closeDevice();
            device_.reset();
            std::cout << "[AUDIO-THREAD] Audio device closed" << std::endl;
        }
        
        // 4. 释放处理器
        if (processor_) {
            std::cout << "[AUDIO-THREAD] Cleaning up audio processor..." << std::endl;
            processor_.reset();
            std::cout << "[AUDIO-THREAD] Audio processor cleaned up" << std::endl;
        }
        
        // 5. 清理波形数据
        latestWaveformData_.clear();
        std::cout << "[AUDIO-THREAD] Waveform data cleared" << std::endl;
        
        // 6. 清理外部回调
        externalAudioCallback_ = nullptr;
        std::cout << "[AUDIO-THREAD] External audio callback cleared" << std::endl;
        
        // 7. 终止 PortAudio
        std::cout << "[AUDIO-THREAD] Terminating PortAudio..." << std::endl;
        PaError err = Pa_Terminate();
        if (err != paNoError) {
            std::cerr << "PortAudio termination failed: " << Pa_GetErrorText(err) << std::endl;
        } else {
            std::cout << "[AUDIO-THREAD] PortAudio terminated successfully" << std::endl;
        }
        
        // 8. 重置状态
        initialized_ = false;
        std::cout << "[AUDIO-THREAD] AudioManager::cleanup: cleanup completed" << std::endl;
    }

    bool writeWavFile(const void* input, size_t frames, const std::string& filename) {
        return writeWavData(filename, input, frames, false);
    }

    bool writeOpusFile(const void* input, size_t frames, const std::string& filename) {
        return writeOpusData(filename, input, frames, false);
    }

    bool readWavFile([[maybe_unused]] const std::string& filename, [[maybe_unused]] std::vector<float>& output, [[maybe_unused]] size_t& frames) {
        // Implementation of readWavFile method
        return false; // Placeholder return, actual implementation needed
    }

    std::string generateOutputFilename([[maybe_unused]] const std::string& format, [[maybe_unused]] int sampleRate, [[maybe_unused]] ChannelCount channels) {
        // Implementation of generateOutputFilename method
        return ""; // Placeholder return, actual implementation needed
    }

    bool startRecording(const std::string& outputFile) {
        std::lock_guard<std::mutex> lock(mutex_);

        // 状态检查
        if (recordingInfo_.state != RecordingState::IDLE && recordingInfo_.state != RecordingState::PREVIEWING) {
            lastError_ = "Not in a valid state to start recording.";
            std::cerr << "[ERROR] " << lastError_ << " Current state: " << static_cast<int>(recordingInfo_.state) << std::endl;
            return false;
        }

        if (outputFile.empty()) {
            lastError_ = "Output file cannot be empty for recording.";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        // 如果是IDLE状态，启动音频流。如果是PREVIEWING，流已经在运行
        if (recordingInfo_.state == RecordingState::IDLE) {
            if (!startAudioStream()) {
                return false;
            }
        }

        // 初始化WAV文件用于写入
        if (!initializeWavFile(outputFile)) {
            // 如果文件初始化失败，并且我们刚启动了流，那么需要停止它
            if (recordingInfo_.state == RecordingState::IDLE) {
                if (audioThread_) {
                    audioThread_->stop();
                }
            }
            return false;
        }
        
        // 更新录音状态
        recordingInfo_.state = RecordingState::RECORDING;
        recordingInfo_.outputFile = outputFile;
        // 当真正开始录音时重置开始时间
        recordingInfo_.startTime = std::chrono::steady_clock::now();
        recordingInfo_.recordedBytes = 0;
        recordingInfo_.recordedFrames = 0;
        recordingInfo_.totalPausedTime = 0.0;
        
        std::cout << "[AUDIO-THREAD] Recording started and saving to: " << outputFile << std::endl;
        return true;
    }

    bool stopRecording() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (recordingInfo_.state == RecordingState::IDLE) {
            return true; // 已经是空闲状态
        }
        
        // 标记为正在停止
        recordingInfo_.state = RecordingState::STOPPING;
        
        // 如果有输出文件，完成WAV文件
        if (!recordingInfo_.outputFile.empty()) {
            finalizeWavFile();
            std::cout << "[AUDIO-THREAD] Recording completed: " << recordingInfo_.outputFile << std::endl;
        }
        
        // 停止音频流
        if (audioThread_) {
            audioThread_->stop();
        }
        
        // 重置录音信息
        recordingInfo_.state = RecordingState::IDLE;
        recordingInfo_.outputFile.clear();
        recordingInfo_.recordedBytes = 0;
        recordingInfo_.recordedFrames = 0;
        recordingInfo_.totalPausedTime = 0.0;
        
        std::cout << "[AUDIO-THREAD] Recording stopped" << std::endl;
        return true;
    }

    bool loadAudioConfig([[maybe_unused]] AudioConfig& inputConfig, [[maybe_unused]] OutputSettings& outputSettings, [[maybe_unused]] const std::string& configPath) {
        // Implementation of loadAudioConfig method
        return false; // Placeholder return, actual implementation needed
    }

    bool saveAudioConfig([[maybe_unused]] const AudioConfig& inputConfig, [[maybe_unused]] const OutputSettings& outputSettings, [[maybe_unused]] const std::string& configPath) {
        // Implementation of saveAudioConfig method
        return false; // Placeholder return, actual implementation needed
    }

    bool parseASRResult(const std::string& jsonStr) {
        (void)jsonStr;
        return false;
    }

    void addLyricSegment(const LyricSegment& segment) {
        (void)segment;
    }

    std::string getCurrentLyric(double timeMs) {
        (void)timeMs;
        return "";
    }

    std::vector<LyricSegment> getAllLyricSegments() const {
        return {};
    }

    std::string getFullTranscriptionText() const {
        return "";
    }

    std::string exportLyricsToLRC() const {
        return "";
    }

    std::string exportLyricsToJSON() const {
        return "";
    }

    bool saveLyricsToFile(const std::string& filePath, const std::string& format) {
        (void)filePath; (void)format;
        return false;
    }

    void clearLyrics() {}

    bool startWritingToFile(const std::string& outputFile) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (recordingInfo_.state != RecordingState::RECORDING) {
            lastError_ = "Stream not started or in invalid state.";
            return false;
        }
        if (!recordingInfo_.outputFile.empty()) {
            lastError_ = "Already writing to a file.";
            return false;
        }

        if (!initializeWavFile(outputFile)) {
            return false;
        }
        recordingInfo_.outputFile = outputFile;
        std::cout << "[AUDIO-THREAD] Started writing to file: " << outputFile << std::endl;
        return true;
    }
    
    bool stopWritingToFile() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (recordingInfo_.outputFile.empty()) {
            return true; // Nothing to do
        }

        finalizeWavFile();
        std::string completedFile = recordingInfo_.outputFile;
        recordingInfo_.outputFile.clear();

        std::cout << "[AUDIO-THREAD] Stopped writing to file: " << completedFile << std::endl;
        return true;
    }

    void setExternalAudioCallback(std::function<void(const void*, void*, size_t)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        externalAudioCallback_ = callback;
    }

    void setOnError(std::function<void(const std::string&)> cb) { onError_ = std::move(cb); }

private:
    // 私有辅助方法
    bool writeWavHeader(const std::string& filename, const WavHeader& header) {
        (void)filename; (void)header;
        return false;
    }

    bool writeWavData(const std::string& filename, const void* data, size_t frames, bool append) {
        (void)filename; (void)data; (void)frames; (void)append;
        return false;
    }

    WavHeader generateWavHeader(size_t dataSize) const {
        WavHeader header;
        memcpy(header.riff, "RIFF", 4);
        header.size = static_cast<uint32_t>(dataSize + 36);
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        header.fmtSize = 16;
        header.format = 1;  // PCM
        header.channels = static_cast<uint16_t>(config_.channels);
        header.sampleRate = static_cast<uint32_t>(config_.sampleRate);
        header.bitsPerSample = 16;  // INT16
        header.blockAlign = header.channels * header.bitsPerSample / 8;
        header.byteRate = header.sampleRate * header.blockAlign;
        memcpy(header.data, "data", 4);
        header.dataSize = static_cast<uint32_t>(dataSize);
        return header;
    }

    bool writeOpusHeader(const std::string& filename, const OpusHeader& header) {
        (void)filename; (void)header;
        return false;
    }

    bool writeOpusData(const std::string& filename, const void* data, size_t frames, bool append) {
        (void)filename; (void)data; (void)frames; (void)append;
        return false;
    }

    OpusHeader generateOpusHeader(size_t dataSize) const {
        (void)dataSize;
        return {};
    }

    // 流式WAV文件写入方法
    bool initializeWavFile(const std::string& filename) {
        // 创建目录
        std::filesystem::path filePath(filename);
        std::filesystem::path parentPath = filePath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath)) {
            try {
                std::filesystem::create_directories(parentPath);
            } catch (const std::exception& e) {
                lastError_ = std::string("Failed to create output directory: ") + e.what();
                return false;
            }
        }
        
        // 打开文件
        wavFile_.open(filename, std::ios::binary);
        if (!wavFile_.is_open()) {
            lastError_ = "Failed to open output file: " + filename;
            return false;
        }
        
        // 写入临时WAV头（数据大小先写0）
        WavHeader header = generateWavHeader(0);
        wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        
        return true;
    }
    
    bool writeWavDataStream(const void* data, size_t frameCount) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        
        if (!wavFile_.is_open()) {
            return false;
        }
        
        // 写入音频数据
        size_t bytesToWrite = frameCount * static_cast<int>(config_.channels) * 2; // INT16 = 2 bytes
        wavFile_.write(static_cast<const char*>(data), bytesToWrite);
        
        // 更新录音信息
        recordingInfo_.recordedBytes += bytesToWrite;
        recordingInfo_.recordedFrames += frameCount;
        
        return true;
    }
    
    void finalizeWavFile() {
        std::lock_guard<std::mutex> lock(fileMutex_);
        
        if (!wavFile_.is_open()) {
            return;
        }
        
        // 计算实际数据大小
        size_t dataSize = recordingInfo_.recordedBytes;
        size_t fileSize = dataSize + sizeof(WavHeader) - 8; // 减去RIFF头中的8字节
        
        // 回到文件开头，更新WAV头
        wavFile_.seekp(0);
        WavHeader header = generateWavHeader(dataSize);
        header.size = static_cast<uint32_t>(fileSize);
        wavFile_.write(reinterpret_cast<const char*>(&header), sizeof(WavHeader));
        
        wavFile_.close();
    }
    
    void updateWaveformData(const void* data, size_t frameCount) {
        // 更新波形数据（用于UI显示）
        const int16_t* samples = static_cast<const int16_t*>(data);
        latestWaveformData_.clear();
        latestWaveformData_.reserve(frameCount);
        
        for (size_t i = 0; i < frameCount; ++i) {
            float normalizedSample = static_cast<float>(samples[i]) / 32768.0f;
            latestWaveformData_.append(normalizedSample);
        }
        
        // 限制波形数据大小，避免内存占用过大
        if (latestWaveformData_.size() > 1000) {
            latestWaveformData_ = latestWaveformData_.mid(latestWaveformData_.size() - 1000);
        }
        
        // 添加调试信息，但限制频率
        static int updateCount = 0;
        static std::chrono::steady_clock::time_point lastUpdateLog = std::chrono::steady_clock::now();
        updateCount++;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateLog).count();
        
        if (elapsed >= 3000) { // 每3秒输出一次调试信息
            std::cout << "[AUDIO-THREAD] Waveform data updated - updates: " << updateCount 
                      << ", frameCount: " << frameCount 
                      << ", waveform size: " << latestWaveformData_.size() << std::endl;
            updateCount = 0;
            lastUpdateLog = now;
        }
    }
    
    bool startAudioStream() {
        if (!device_ || !device_->isDeviceOpen()) {
            lastError_ = "No audio device available";
            return false;
        }
        
        // 启动音频流
        if (!device_->startStream()) {
            lastError_ = "Failed to start audio stream: " + device_->getLastError();
            return false;
        }
        
        return true;
    }

    /**
     * @brief 音频回调函数
     * @param input 输入音频数据
     * @param output 输出音频数据
     * @param frameCount 帧数
     */
    void audioCallback(const void* input, void* output, size_t frameCount) {
        if (!initialized_) {
            std::cerr << "[ERROR] Audio callback called before initialization" << std::endl;
            return;
        }

        // 添加调试信息，但限制频率避免日志过多
        static int callbackCount = 0;
        static std::chrono::steady_clock::time_point lastLogTime = std::chrono::steady_clock::now();
        callbackCount++;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastLogTime).count();
        
        if (elapsed >= 5000) { // 每5秒输出一次调试信息
            std::cout << "[AUDIO-THREAD] Audio callback active - callbacks: " << callbackCount 
                      << ", frameCount: " << frameCount 
                      << ", input: " << (input ? "valid" : "null") << std::endl;
            callbackCount = 0;
            lastLogTime = now;
        }

        // 1. 波形数据处理（现有逻辑）
        if (input && frameCount > 0) {
            updateWaveformData(input, frameCount);
        }

        // 2. WAV文件写入（现有逻辑）
        if (recordingInfo_.state == RecordingState::RECORDING && input && !recordingInfo_.outputFile.empty()) {
            writeWavDataStream(input, frameCount);
        }

        // 3. 外部音频回调（新增：用于实时ASR）
        if (externalAudioCallback_) {
            externalAudioCallback_(input, output, frameCount);
        }

        // 4. 输出处理（现有逻辑）
        if (output) {
            // 输出处理逻辑
        }
    }

    //--------------------------------------------------------------------------
    // 成员变量
    //--------------------------------------------------------------------------

    bool initialized_;
    std::shared_ptr<AudioProcessor> processor_;
    std::unique_ptr<AudioThread> audioThread_;
    std::string currentOutputFile_;
    std::vector<int16_t> recordingBuffer_;
    std::unique_ptr<AudioDevice> device_;
    AudioConfig currentConfig_;
    std::string lastError_;
    mutable std::mutex mutex_;
    LyricSyncManager lyricSyncManager_;
    
    // 流式录音相关成员变量
    RecordingInfo recordingInfo_;
    std::ofstream wavFile_;
    std::mutex fileMutex_;
    QVector<float> latestWaveformData_;
    
    // 外部音频回调
    std::function<void(const void*, void*, size_t)> externalAudioCallback_;
    std::function<void(const std::string&)> onError_;
};

// AudioManager构造函数和析构函数
AudioManager::AudioManager() : impl_(std::make_unique<Impl>()) {
    impl_->setOnError([this](const std::string& err) {
        emitError(QString::fromStdString(err));
    });
}

AudioManager::~AudioManager() = default;

// AudioManager公共接口实现
bool AudioManager::initialize(const AudioConfig& config) {
    return impl_->initialize(config);
}

const AudioConfig& AudioManager::getConfig() const {
    return impl_->config_;
}

bool AudioManager::updateConfig(const AudioConfig& config) {
    impl_->cleanup();
    return impl_->initialize(config);
}

std::vector<DeviceInfo> AudioManager::getAvailableDevices() {
    return impl_->getAvailableDevices();
}

std::shared_ptr<AudioThread> AudioManager::createAudioThread(const AudioConfig& config) {
    return impl_->createAudioThread(config);
}

std::shared_ptr<AudioProcessor> AudioManager::getProcessor() {
    return impl_->getProcessor();
}

void AudioManager::cleanup() {
    impl_->cleanup();
}

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
    return impl_->loadAudioConfig(inputConfig, outputSettings, configPath);
}

bool AudioManager::saveAudioConfig(const AudioConfig& inputConfig, const OutputSettings& outputSettings, const std::string& configPath) {
    return impl_->saveAudioConfig(inputConfig, outputSettings, configPath);
}

// ============================================================================
// 歌词同步格式相关方法实现
// ============================================================================

bool AudioManager::updateLyricSyncFromASR(const std::string& asrResult) {
    return impl_->parseASRResult(asrResult);
}

void AudioManager::addLyricSegment(const LyricSegment& segment) {
    impl_->addLyricSegment(segment);
}

std::string AudioManager::getCurrentLyric(double timeMs) {
    return impl_->getCurrentLyric(timeMs);
}

std::vector<LyricSegment> AudioManager::getAllLyricSegments() const {
    return impl_->getAllLyricSegments();
}

std::string AudioManager::getFullTranscriptionText() const {
    return impl_->getFullTranscriptionText();
}

std::string AudioManager::exportLyricsToLRC() const {
    return impl_->exportLyricsToLRC();
}

std::string AudioManager::exportLyricsToJSON() const {
    return impl_->exportLyricsToJSON();
}

bool AudioManager::saveLyricsToFile(const std::string& filePath, const std::string& format) {
    return impl_->saveLyricsToFile(filePath, format);
}

void AudioManager::clearLyrics() {
    impl_->clearLyrics();
}

bool AudioManager::startWritingToFile(const std::string& outputFile) {
    return impl_->startWritingToFile(outputFile);
}

bool AudioManager::stopWritingToFile() {
    return impl_->stopWritingToFile();
}

bool AudioManager::pauseRecording() {
    return impl_->pauseStreamRecording();
}

bool AudioManager::resumeRecording() {
    return impl_->resumeStreamRecording();
}

bool AudioManager::startStreamRecording(const std::string& outputFile) {
    return impl_->startStreamRecording(outputFile);
}

bool AudioManager::stopStreamRecording() {
    return impl_->stopStreamRecording();
}

size_t AudioManager::getRecordedBytes() const {
    return impl_->getRecordedBytes();
}

QVector<float> AudioManager::getLatestWaveformData() const {
    return impl_->getLatestWaveformData();
}

void AudioManager::setExternalAudioCallback(std::function<void(const void*, void*, size_t)> callback) {
    impl_->setExternalAudioCallback(callback);
}

} // namespace audio
} // namespace perfx 