#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace perfx {
namespace audio {

struct AudioFormat {
    int sampleRate;
    int channels;
    int bitsPerSample;
    std::string format;
};

struct ConversionProgress {
    double progress;  // 0.0 to 1.0
    AudioFormat sourceFormat;
    AudioFormat targetFormat;
    size_t processedBytes;
    size_t totalBytes;
};

class AudioConverter {
public:
    AudioConverter();
    ~AudioConverter();

    // 设置回调函数来接收转换进度
    using ProgressCallback = std::function<void(const ConversionProgress&)>;
    void setProgressCallback(ProgressCallback callback);

    // 开始转换
    bool startConversion(const std::string& inputFile, const std::string& outputFile);
    
    // 停止转换
    void stopConversion();

    // 获取当前转换状态
    ConversionProgress getCurrentProgress() const;

    // 获取最后一次错误信息
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace audio
} // namespace perfx 