#pragma once

#include "audio_types.h"
#include <memory>
#include <vector>

namespace perfx {
namespace audio {

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // 初始化处理器
    bool initialize(const AudioConfig& config);
    
    // 重采样相关函数
    bool startResampling(SampleRate fromRate, SampleRate toRate);
    bool processResampling(const void* input, size_t inputFrames,
                          void* output, size_t& outputFrames);
    void stopResampling();
    bool resample(const void* input, size_t inputFrames,
                 void* output, size_t outputFrames,
                 SampleRate fromRate, SampleRate toRate);

    // Opus 编码解码相关函数
    bool encodeOpus(const void* input, size_t frames, std::vector<uint8_t>& output);
    bool decodeOpus(const std::vector<uint8_t>& input, void* output, size_t& outputFrames);

    // 获取处理后的配置
    AudioConfig getProcessedConfig() const;

    // 设置编码格式
    void setEncodingFormat(EncodingFormat format);
    void setOpusFrameLength(int frameLength);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 