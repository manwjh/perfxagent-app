#pragma once

#include "audio_types.h"
#include <memory>
#include <vector>
#include <string>

namespace perfx {
namespace audio {

class AudioProcessor {
public:
    AudioProcessor();
    ~AudioProcessor();

    // 初始化处理器
    bool initialize(const AudioConfig& config);
    
    // 重采样
    bool resample(const void* input, size_t inputFrames, 
                 void* output, size_t outputFrames,
                 SampleRate fromRate, SampleRate toRate);
    
    // Opus 编码
    bool encodeOpus(const void* input, size_t frames, 
                   std::vector<uint8_t>& output);
    
    // Opus 解码
    bool decodeOpus(const std::vector<uint8_t>& input,
                   void* output, size_t& outputFrames);
    
    // WAV 写入
    bool writeWav(const void* input, size_t frames,
                 const std::string& filename);
    
    // WAV 读取
    bool readWav(const std::string& filename,
                std::vector<float>& output,
                size_t& frames);
    
    // 获取处理后的音频配置
    AudioConfig getProcessedConfig() const;

    // 流式重采样接口
    bool startResampling(SampleRate fromRate, SampleRate toRate);
    bool processResampling(const void* input, size_t inputFrames,
                          void* output, size_t& outputFrames);
    void stopResampling();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace audio
} // namespace perfx 