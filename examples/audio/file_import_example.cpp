#include "audio/file_importer.h"
#include <iostream>
#include <string>
#include <vector>

using namespace perfx::audio;

int main() {
    std::cout << "🎵 PerfX Audio File Importer" << std::endl;
    std::cout << "===========================" << std::endl;

    // 创建文件导入器实例
    FileImporter importer;

    // 获取目标目录
    std::string targetDir;
    std::cout << "\nEnter target directory path: ";
    std::getline(std::cin, targetDir);

    // 获取要导入的文件路径
    std::vector<std::string> filePaths;
    std::cout << "\nEnter audio file paths (one per line, empty line to finish):" << std::endl;
    
    std::string filePath;
    while (true) {
        std::getline(std::cin, filePath);
        if (filePath.empty()) {
            break;
        }
        filePaths.push_back(filePath);
    }

    if (filePaths.empty()) {
        std::cout << "No files specified." << std::endl;
        return 1;
    }

    // 导入文件
    std::cout << "\nImporting files..." << std::endl;
    size_t successCount = importer.importAudioFiles(filePaths, targetDir);

    // 显示结果
    std::cout << "\nImport completed:" << std::endl;
    std::cout << "Successfully imported: " << successCount << " files" << std::endl;
    std::cout << "Failed to import: " << (filePaths.size() - successCount) << " files" << std::endl;

    if (successCount < filePaths.size()) {
        std::cout << "\nLast error: " << importer.getLastError() << std::endl;
    }

    return 0;
} 