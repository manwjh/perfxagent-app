#include "audio/file_importer.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace perfx {
namespace audio {

FileImporter::FileImporter() = default;
FileImporter::~FileImporter() = default;

bool FileImporter::isValidAudioFile(const std::string& filePath) {
    // 检查文件是否存在
    if (!std::filesystem::exists(filePath)) {
        lastError_ = "File does not exist: " + filePath;
        return false;
    }

    // 检查文件扩展名
    std::string extension = std::filesystem::path(filePath).extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    // 支持的音频文件格式
    const std::vector<std::string> supportedFormats = {
        ".wav", ".mp3", ".ogg", ".flac", ".m4a", ".aac"
    };

    if (std::find(supportedFormats.begin(), supportedFormats.end(), extension) == supportedFormats.end()) {
        lastError_ = "Unsupported audio format: " + extension;
        return false;
    }

    // 对于WAV文件，检查文件头
    if (extension == ".wav") {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            lastError_ = "Failed to open file: " + filePath;
            return false;
        }

        char header[12];
        file.read(header, sizeof(header));
        file.close();

        // 检查WAV文件头
        if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
            lastError_ = "Invalid WAV file header";
            return false;
        }
    }

    return true;
}

bool FileImporter::createTargetDirectory(const std::string& dirPath) {
    try {
        if (!std::filesystem::exists(dirPath)) {
            return std::filesystem::create_directories(dirPath);
        }
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to create directory: ") + e.what();
        return false;
    }
}

bool FileImporter::copyFile(const std::string& sourcePath, const std::string& targetPath) {
    try {
        std::filesystem::copy_file(sourcePath, targetPath, 
                                 std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to copy file: ") + e.what();
        return false;
    }
}

std::string FileImporter::getSavedFileDirectory(const std::string& sourcePath, const std::string& targetDir) const {
    // 获取文件名（不含扩展名）
    std::string fileName = std::filesystem::path(sourcePath).stem().string();
    
    // 创建目标目录路径
    std::string targetPath = std::filesystem::path(targetDir) / fileName;
    
    // 返回完整的目标目录路径
    return std::filesystem::absolute(targetPath).string();
}

bool FileImporter::importAudioFile(const std::string& sourcePath, const std::string& targetDir) {
    // 验证源文件
    if (!isValidAudioFile(sourcePath)) {
        return false;
    }

    // 获取文件名（不含扩展名）
    std::string fileName = std::filesystem::path(sourcePath).stem().string();
    
    // 创建目标目录
    std::string targetPath = std::filesystem::path(targetDir) / fileName;
    if (!createTargetDirectory(targetPath)) {
        return false;
    }

    // 复制文件到目标目录
    std::string targetFilePath = (std::filesystem::path(targetPath) / 
                                std::filesystem::path(sourcePath).filename()).string();
    
    if (copyFile(sourcePath, targetFilePath)) {
        // 显示保存目录信息
        std::cout << "\n=== 文件保存信息 ===" << std::endl;
        std::cout << "文件已保存到目录: " << targetPath << std::endl;
        std::cout << "完整文件路径: " << targetFilePath << std::endl;
        std::cout << "该目录将作为该文件的工作目录" << std::endl;
        return true;
    }
    
    return false;
}

size_t FileImporter::importAudioFiles(const std::vector<std::string>& sourcePaths, 
                                    const std::string& targetDir) {
    size_t successCount = 0;
    
    for (const auto& sourcePath : sourcePaths) {
        if (importAudioFile(sourcePath, targetDir)) {
            successCount++;
        } else {
            std::cerr << "Failed to import file: " << sourcePath << std::endl;
            std::cerr << "Error: " << lastError_ << std::endl;
        }
    }
    
    return successCount;
}

} // namespace audio
} // namespace perfx 