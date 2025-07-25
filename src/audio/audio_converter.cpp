#include "audio/audio_converter.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <memory>
#include <thread>

namespace perfx {
namespace audio {

class AudioConverter::Impl {
public:
    Impl() : isConverting_(false) {
        std::cout << "[AUDIO-THREAD] Initializing AudioConverter::Impl..." << std::endl;
        std::cout << "[AUDIO-THREAD] AudioConverter::Impl initialization completed" << std::endl;
    }

    ~Impl() {
        std::cout << "[AUDIO-THREAD] Destroying AudioConverter::Impl..." << std::endl;
        std::cout << "[AUDIO-THREAD] AudioConverter::Impl destroyed" << std::endl;
    }

    bool startConversion(const std::string& inputFile, const std::string& outputFile) {
        std::cout << "[AUDIO-THREAD] Starting conversion..." << std::endl;
        std::cout << "[AUDIO-THREAD] Input file: " << inputFile << std::endl;
        std::cout << "[AUDIO-THREAD] Output file: " << outputFile << std::endl;
        
        if (isConverting_) {
            lastError_ = "Already converting";
            std::cerr << "[AUDIO-THREAD][ERROR] " << lastError_ << std::endl;
            return false;
        }

        isConverting_ = true;
        currentProgress_ = ConversionProgress{};
        
        // 检查输入文件是否存在
        if (!std::filesystem::exists(inputFile)) {
            lastError_ = "Input file does not exist: " + inputFile;
            std::cerr << "[AUDIO-THREAD][ERROR] " << lastError_ << std::endl;
            return false;
        }
        
        // 检查输出目录是否存在
        std::filesystem::path outputPath(outputFile);
        std::filesystem::path outputDir = outputPath.parent_path();
        if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
            try {
                std::filesystem::create_directories(outputDir);
            } catch (const std::exception& e) {
                lastError_ = "Failed to create output directory: " + std::string(e.what());
                std::cerr << "[AUDIO-THREAD][ERROR] Conversion failed: " << e.what() << std::endl;
                return false;
            }
        }
        
        // 启动转换线程
        std::cout << "[AUDIO-THREAD] Conversion thread started" << std::endl;
        conversionThread_ = std::thread([this, inputFile, outputFile]() {
            try {
                convertFile(inputFile, outputFile);
            } catch (const std::exception& e) {
                lastError_ = "Conversion thread exception: " + std::string(e.what());
                std::cerr << "[AUDIO-THREAD][ERROR] " << lastError_ << std::endl;
            }
            isConverting_ = false;
            std::cout << "[AUDIO-THREAD] Conversion thread finished" << std::endl;
        });
        
        std::cout << "[AUDIO-THREAD] Conversion thread created and detached" << std::endl;
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
    void convertFile(const std::string& inputFile, const std::string& outputFile) {
        try {
            std::cout << "[DEBUG] Starting file conversion..." << std::endl;

            // 确保输出目录存在
            std::filesystem::path outputPath(outputFile);
            std::error_code ec;
            std::filesystem::create_directories(outputPath.parent_path(), ec);
            if (ec) {
                lastError_ = "Failed to create output directory: " + ec.message();
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return;
            }

            // 更新进度信息
            currentProgress_.sourceFormat.sampleRate = 16000;
            currentProgress_.sourceFormat.channels = 1;
            currentProgress_.sourceFormat.bitsPerSample = 16;
            currentProgress_.sourceFormat.format = "wav";
            currentProgress_.totalBytes = 100;
            currentProgress_.processedBytes = 0;
            currentProgress_.progress = 0.0;

            if (progressCallback_) {
                std::cout << "[DEBUG] Calling initial progress callback" << std::endl;
                progressCallback_(currentProgress_);
            }

            // 使用FFmpeg直接转换为目标格式
            std::string cmd = "ffmpeg -y -i \"" + inputFile + "\" -acodec pcm_s16le -ar 16000 -ac 1 \"" + outputFile + "\"";
            std::cout << "[DEBUG] Running FFmpeg command: " << cmd << std::endl;

            int result = std::system(cmd.c_str());
            std::cout << "[DEBUG] FFmpeg command returned: " << result << std::endl;

            if (result != 0) {
                lastError_ = "FFmpeg conversion failed with code: " + std::to_string(result);
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return;
            }

            // 检查输出文件是否存在
            if (!std::filesystem::exists(outputFile)) {
                lastError_ = "Output file was not created: " + outputFile;
                std::cerr << "[ERROR] " << lastError_ << std::endl;
                return;
            }

            // 获取文件大小
            std::filesystem::path outputFilePath(outputFile);
            currentProgress_.totalBytes = std::filesystem::file_size(outputFilePath);

            // 设置最终进度
            currentProgress_.progress = 1.0;
            currentProgress_.processedBytes = currentProgress_.totalBytes;

            if (progressCallback_) {
                std::cout << "[DEBUG] Calling final progress callback" << std::endl;
                progressCallback_(currentProgress_);
            }

            std::cout << "[DEBUG] Conversion completed successfully" << std::endl;
            std::cout << "[DEBUG] Output file: " << outputFile << std::endl;
        } catch (const std::exception& e) {
            lastError_ = std::string("Exception in convertFile: ") + e.what();
            std::cerr << "[ERROR] " << lastError_ << std::endl;
        } catch (...) {
            lastError_ = "Unknown exception in convertFile";
            std::cerr << "[ERROR] " << lastError_ << std::endl;
        }
    }

    bool isConverting_;
    std::string lastError_;
    ConversionProgress currentProgress_;
    ProgressCallback progressCallback_;
    std::thread conversionThread_;
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