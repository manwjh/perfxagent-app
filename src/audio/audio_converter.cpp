#include "audio/audio_converter.h"
#include "audio/audio_manager.h"
#include "audio/audio_types.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <memory>

namespace perfx {
namespace audio {

class AudioConverter::Impl {
public:
    Impl() : isConverting_(false) {
        std::cout << "[DEBUG] Initializing AudioConverter::Impl..." << std::endl;
        
        // 初始化 AudioManager
        audioManager_ = std::make_unique<AudioManager>();
        if (!audioManager_) {
            std::cerr << "[ERROR] Failed to create AudioManager" << std::endl;
            return;
        }
        
        // 配置音频参数
        AudioConfig config;
        config.sampleRate = SampleRate::RATE_16000;  // 16kHz
        config.channels = ChannelCount::MONO;        // 单声道
        config.format = SampleFormat::INT16;
        config.framesPerBuffer = 256;
        config.encodingFormat = EncodingFormat::OPUS;
        config.opusFrameLength = 20;
        config.opusBitrate = 128000;
        config.opusComplexity = 10;
        
        // 初始化 AudioManager，禁用输入设备
        config.inputDevice = DeviceInfo{};  // 使用空的 DeviceInfo 来禁用输入设备
        if (!audioManager_->initialize(config)) {
            std::cerr << "[ERROR] Failed to initialize AudioManager: " << audioManager_->getLastError() << std::endl;
            return;
        }
        
        std::cout << "[DEBUG] AudioConverter::Impl initialization completed" << std::endl;
    }

    ~Impl() {
        std::cout << "[DEBUG] Destroying AudioConverter::Impl..." << std::endl;
        audioManager_.reset();
        std::cout << "[DEBUG] AudioConverter::Impl destroyed" << std::endl;
    }

    bool startConversion(const std::string& inputFile, const std::string& outputFile) {
        std::cout << "[DEBUG] Starting conversion..." << std::endl;
        std::cout << "[DEBUG] Input file: " << inputFile << std::endl;
        std::cout << "[DEBUG] Output file: " << outputFile << std::endl;
        
        if (isConverting_) {
            lastError_ = "Already converting";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        if (!audioManager_) {
            lastError_ = "AudioManager not initialized";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
            return false;
        }

        isConverting_ = true;
        currentProgress_ = ConversionProgress{};
        
        // 创建转换线程
        std::thread conversionThread([this, inputFile, outputFile]() {
            std::cout << "[DEBUG] Conversion thread started" << std::endl;
            try {
                convertFile(inputFile, outputFile);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Conversion failed: " << e.what() << std::endl;
                lastError_ = e.what();
            }
            isConverting_ = false;
            std::cout << "[DEBUG] Conversion thread finished" << std::endl;
        });
        conversionThread.detach();
        
        std::cout << "[DEBUG] Conversion thread created and detached" << std::endl;
        return true;
    }

    void stopConversion() {
        isConverting_ = false;
    }

    void setProgressCallback(ProgressCallback callback) {
        progressCallback_ = std::move(callback);
    }

    ConversionProgress getCurrentProgress() const {
        return currentProgress_;
    }

    std::string getLastError() const {
        return lastError_;
    }

private:
    std::string convertToWav(const std::string& inputFile) {
        std::cout << "[DEBUG] Converting to WAV format..." << std::endl;
        
        // 创建临时 WAV 文件路径
        std::filesystem::path tempPath = std::filesystem::path(inputFile).parent_path() / 
                                       (std::filesystem::path(inputFile).stem().string() + "_temp.wav");
        std::string tempFile = tempPath.string();
        
        // 构建 FFmpeg 命令，直接转换为 16kHz 单声道
        std::string cmd = "ffmpeg -y -i \"" + inputFile + "\" -acodec pcm_s16le -ar 16000 -ac 1 \"" + tempFile + "\"";
        std::cout << "[DEBUG] Running command: " << cmd << std::endl;
        
        // 执行 FFmpeg 命令
        int result = std::system(cmd.c_str());
        if (result != 0) {
            std::string error = "FFmpeg conversion failed with code: " + std::to_string(result);
            std::cerr << "[ERROR] " << error << std::endl;
            throw std::runtime_error(error);
        }
        
        std::cout << "[DEBUG] WAV conversion completed successfully" << std::endl;
        return tempFile;
    }

    void convertFile(const std::string& inputFile, const std::string& outputFile) {
        std::cout << "[DEBUG] Opening input file: " << inputFile << std::endl;
        
        // 首先转换为 WAV 格式
        std::string wavFile = convertToWav(inputFile);
        std::cout << "[DEBUG] Using temporary WAV file: " << wavFile << std::endl;
        
        // 读取 WAV 文件
        std::vector<float> audioData;
        size_t frames;
        if (!audioManager_->readWavFile(wavFile, audioData, frames)) {
            std::string error = "Failed to read WAV file: " + audioManager_->getLastError();
            std::cerr << "[ERROR] " << error << std::endl;
            lastError_ = error;
            return;
        }
        
        std::cout << "[DEBUG] WAV file read successfully" << std::endl;
        std::cout << "[DEBUG] Frames: " << frames << std::endl;
        
        // 更新进度信息
        currentProgress_.sourceFormat.sampleRate = 16000;  // 16kHz
        currentProgress_.sourceFormat.channels = 1;        // 单声道
        currentProgress_.sourceFormat.bitsPerSample = 16;  // 16位
        currentProgress_.sourceFormat.format = "wav";
        currentProgress_.totalBytes = frames * sizeof(float);  // 单声道，所以不需要乘以2
        currentProgress_.processedBytes = 0;
        currentProgress_.progress = 0.0;
        
        if (progressCallback_) {
            std::cout << "[DEBUG] Calling initial progress callback" << std::endl;
            progressCallback_(currentProgress_);
        }
        
        // 转换为 OGG/Opus 格式
        std::cout << "[DEBUG] Converting to OGG/Opus format..." << std::endl;
        if (!audioManager_->writeOpusFile(audioData.data(), frames, outputFile)) {
            std::string error = "Failed to write OGG/Opus file: " + audioManager_->getLastError();
            std::cerr << "[ERROR] " << error << std::endl;
            lastError_ = error;
            return;
        }
        
        // 删除临时 WAV 文件
//        std::filesystem::remove(wavFile);
        
//        std::cout << "[DEBUG] Files closed and temporary file removed" << std::endl;
        
        // 设置最终进度
        currentProgress_.progress = 1.0;
        if (progressCallback_) {
            std::cout << "[DEBUG] Calling final progress callback" << std::endl;
            progressCallback_(currentProgress_);
        }
        
        std::cout << "[DEBUG] Conversion completed successfully" << std::endl;
    }

    bool isConverting_;
    std::string lastError_;
    ConversionProgress currentProgress_;
    ProgressCallback progressCallback_;
    std::unique_ptr<AudioManager> audioManager_;
};

// AudioConverter implementation
AudioConverter::AudioConverter() : pImpl_(std::make_unique<Impl>()) {
    std::cout << "[DEBUG] AudioConverter created" << std::endl;
}

AudioConverter::~AudioConverter() = default;

void AudioConverter::setProgressCallback(ProgressCallback callback) {
    pImpl_->setProgressCallback(std::move(callback));
}

bool AudioConverter::startConversion(const std::string& inputFile, const std::string& outputFile) {
    return pImpl_->startConversion(inputFile, outputFile);
}

void AudioConverter::stopConversion() {
    pImpl_->stopConversion();
}

ConversionProgress AudioConverter::getCurrentProgress() const {
    return pImpl_->getCurrentProgress();
}

std::string AudioConverter::getLastError() const {
    return pImpl_->getLastError();
}

} // namespace audio
} // namespace perfx 