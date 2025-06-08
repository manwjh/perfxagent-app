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
    void updateConfig(const AudioConfig& config);
    const AudioConfig& getConfig() const;
    void processAudio(const void* input, void* output, unsigned long frameCount);
    
    // Opus 编码解码相关函数
    bool encodeOpus(const void* input, size_t frames, std::vector<std::vector<uint8_t>>& encodedFrames);
    bool decodeOpus(const std::vector<uint8_t>& input, void* output, size_t& outputFrames);

    // Opus 帧封装相关函数
    bool encapsulateOpusFrame(const std::vector<uint8_t>& encodedData, std::vector<uint8_t>& output);
    bool decapsulateOpusFrame(const std::vector<uint8_t>& input, std::vector<uint8_t>& output);
    bool createOpusHeader(std::vector<uint8_t>& header);

    // 获取处理后的配置
    AudioConfig getProcessedConfig() const;

    // 设置编码格式
    void setEncodingFormat(EncodingFormat format);
    void setOpusFrameLength(int frameLength);

    // VAD 相关函数
    bool initializeVAD(const VADConfig& config);
    bool isVoiceActive(const float* input, size_t frameCount);
    bool isVoiceActive(const int16_t* input, size_t frameCount);
    void setVADThreshold(float threshold);
    float getVADThreshold() const;
    const VADStatus& getVADStatus() const;
    void updateVADConfig(const VADConfig& config);
    const VADConfig& getVADConfig() const;
    bool shouldInsertSilenceFrame() const;
    bool isSentenceEnd() const;
    bool isIdle() const;

    int getSampleRate() const { return static_cast<int>(config_.sampleRate); }
    int getChannels() const { return static_cast<int>(config_.channels); }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    AudioConfig config_;
};

} // namespace audio
} // namespace perfx 