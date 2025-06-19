#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

namespace perfx {
namespace audio {

/**
 * @brief 文件导入器类
 * 负责处理音频文件的导入、验证和存储
 */
class FileImporter {
public:
    FileImporter();
    ~FileImporter();

    /**
     * @brief 检查文件是否为有效的音频文件
     * @param filePath 文件路径
     * @return 是否为有效的音频文件
     */
    bool isValidAudioFile(const std::string& filePath);

    /**
     * @brief 导入音频文件
     * @param sourcePath 源文件路径
     * @param targetDir 目标目录
     * @return 导入是否成功
     */
    bool importAudioFile(const std::string& sourcePath, const std::string& targetDir);

    /**
     * @brief 批量导入音频文件
     * @param sourcePaths 源文件路径列表
     * @param targetDir 目标目录
     * @return 成功导入的文件数量
     */
    size_t importAudioFiles(const std::vector<std::string>& sourcePaths, const std::string& targetDir);

    /**
     * @brief 获取最后一次错误信息
     * @return 错误信息字符串
     */
    std::string getLastError() const { return lastError_; }

    /**
     * @brief 获取保存文件的目录信息
     * @param sourcePath 源文件路径
     * @param targetDir 目标目录
     * @return 保存文件的完整目录路径
     */
    std::string getSavedFileDirectory(const std::string& sourcePath, const std::string& targetDir) const;

private:
    /**
     * @brief 创建目标目录
     * @param dirPath 目录路径
     * @return 是否创建成功
     */
    bool createTargetDirectory(const std::string& dirPath);

    /**
     * @brief 复制文件
     * @param sourcePath 源文件路径
     * @param targetPath 目标文件路径
     * @return 是否复制成功
     */
    bool copyFile(const std::string& sourcePath, const std::string& targetPath);

    std::string lastError_;  ///< 最后一次错误信息
};

} // namespace audio
} // namespace perfx 