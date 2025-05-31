#include "../../include/audio/audio_processor.h"
#include <opus/opus.h>
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}
#include <samplerate.h>
// #include <webrtc/common_audio/vad/include/webrtc_vad.h>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <iostream>

namespace perfx {
namespace audio {

// WAV 文件头结构
struct WavHeader {
    char riff[4];        // "RIFF"
    uint32_t size;       // 文件大小 - 8
    char wave[4];        // "WAVE"
    char fmt[4];         // "fmt "
    uint32_t fmtSize;    // fmt 块大小
    uint16_t format;     // 1 = PCM
    uint16_t channels;   // 声道数
    uint32_t sampleRate; // 采样率
    uint32_t byteRate;   // 每秒字节数
    uint16_t blockAlign; // 块对齐
    uint16_t bitsPerSample; // 位深度
    char data[4];        // "data"
    uint32_t dataSize;   // 数据大小
};

class AudioProcessor::Impl {
public:
    Impl() : opusEncoder_(nullptr), opusDecoder_(nullptr), swrCtx_(nullptr) {}

    ~Impl() {
        cleanup();
    }

    bool initialize(const AudioConfig& config) {
        cleanup();
        config_ = config;

        // 打印配置信息，用于调试
        std::cout << "AudioProcessor initialized with:" << std::endl;
        std::cout << "- Sample Rate: " << static_cast<int>(config_.sampleRate) << " Hz" << std::endl;
        std::cout << "- Channels: " << (config_.channels == ChannelCount::MONO ? "MONO" : "STEREO") << std::endl;
        std::cout << "- Format: " << (config_.format == SampleFormat::FLOAT32 ? "FLOAT32" : "INT16") << std::endl;

        // 初始化 Opus 编码器
        int error;
        opusEncoder_ = opus_encoder_create(
            static_cast<int>(config.sampleRate),
            static_cast<int>(config.channels),
            OPUS_APPLICATION_VOIP,
            &error
        );
        if (error != OPUS_OK || !opusEncoder_) {
            throw std::runtime_error("Failed to create Opus encoder");
        }

        // 初始化 Opus 解码器
        opusDecoder_ = opus_decoder_create(
            static_cast<int>(config.sampleRate),
            static_cast<int>(config.channels),
            &error
        );
        if (error != OPUS_OK || !opusDecoder_) {
            throw std::runtime_error("Failed to create Opus decoder");
        }

        return true;
    }

    bool startResampling(SampleRate fromRate, SampleRate toRate) {
        if (swrCtx_) {
            swr_free(&swrCtx_);
        }

        // 创建重采样上下文
        swrCtx_ = swr_alloc();
        if (!swrCtx_) {
            std::cerr << "Failed to allocate resampling context" << std::endl;
            return false;
        }

        // 设置重采样参数
        av_opt_set_int(swrCtx_, "in_channel_count", static_cast<int>(config_.channels), 0);
        av_opt_set_int(swrCtx_, "out_channel_count", static_cast<int>(config_.channels), 0);
        av_opt_set_int(swrCtx_, "in_channel_layout", AV_CH_LAYOUT_MONO, 0);
        av_opt_set_int(swrCtx_, "out_channel_layout", AV_CH_LAYOUT_MONO, 0);
        av_opt_set_int(swrCtx_, "in_sample_rate", static_cast<int>(fromRate), 0);
        av_opt_set_int(swrCtx_, "out_sample_rate", static_cast<int>(toRate), 0);
        av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

        // 初始化重采样上下文
        int ret = swr_init(swrCtx_);
        if (ret < 0) {
            std::cerr << "Failed to initialize resampling context" << std::endl;
            swr_free(&swrCtx_);
            swrCtx_ = nullptr;
            return false;
        }

        return true;
    }

    bool processResampling(const void* input, size_t inputFrames,
                          void* output, size_t& outputFrames) {
        if (!swrCtx_ || !input || !output) {
            return false;
        }

        // 计算输出帧数
        outputFrames = av_rescale_rnd(
            swr_get_delay(swrCtx_, static_cast<int>(config_.sampleRate)) + inputFrames,
            static_cast<int>(config_.sampleRate),
            static_cast<int>(config_.sampleRate),
            AV_ROUND_UP
        );

        // 执行重采样
        const uint8_t** in = reinterpret_cast<const uint8_t**>(&input);
        uint8_t** out = reinterpret_cast<uint8_t**>(&output);
        int ret = swr_convert(swrCtx_, out, outputFrames, in, inputFrames);
        
        if (ret < 0) {
            std::cerr << "Error during resampling" << std::endl;
            return false;
        }

        outputFrames = ret;
        return true;
    }

    void stopResampling() {
        if (swrCtx_) {
            swr_free(&swrCtx_);
            swrCtx_ = nullptr;
        }
    }

    bool resample(const void* input, size_t inputFrames,
                 void* output, size_t outputFrames,
                 SampleRate fromRate, SampleRate toRate) {
        if (!input || !output) return false;

        // 使用 FFmpeg 的重采样
        if (!startResampling(fromRate, toRate)) {
            return false;
        }

        size_t actualOutputFrames = outputFrames;
        bool result = processResampling(input, inputFrames, output, actualOutputFrames);
        
        stopResampling();
        return result;
    }

    bool encodeOpus(const void* input, size_t frames,
                   std::vector<uint8_t>& output) {
        if (!opusEncoder_ || !input) return false;

        // 转换为 16-bit PCM
        std::vector<int16_t> pcmData(frames * static_cast<int>(config_.channels));
        convertToInt16(input, pcmData.data(), frames);

        // 计算最大输出大小
        const int maxFrameSize = 1275; // Opus 最大帧大小
        output.resize(maxFrameSize);

        // 编码
        int encodedBytes = opus_encode(
            opusEncoder_,
            pcmData.data(),
            frames,
            output.data(),
            maxFrameSize
        );

        if (encodedBytes < 0) {
            return false;
        }

        output.resize(encodedBytes);
        return true;
    }

    bool decodeOpus(const std::vector<uint8_t>& input,
                   void* output, size_t& outputFrames) {
        if (!opusDecoder_ || !output || input.empty()) return false;

        // 解码
        int decodedFrames = opus_decode(
            opusDecoder_,
            input.data(),
            input.size(),
            static_cast<int16_t*>(output),
            outputFrames,
            0
        );

        if (decodedFrames < 0) {
            return false;
        }

        outputFrames = decodedFrames;
        return true;
    }

    bool writeWav(const void* input, size_t frames, const std::string& filename) {
        if (!input || frames == 0) return false;

        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        // 准备 WAV 头
        WavHeader header = {};
        memcpy(header.riff, "RIFF", 4);
        memcpy(header.wave, "WAVE", 4);
        memcpy(header.fmt, "fmt ", 4);
        memcpy(header.data, "data", 4);

        header.fmtSize = 16;
        header.format = 1; // PCM
        header.channels = static_cast<uint16_t>(config_.channels);
        header.sampleRate = static_cast<uint32_t>(config_.sampleRate);
        header.bitsPerSample = 16; // 16-bit PCM
        header.blockAlign = header.channels * header.bitsPerSample / 8;
        header.byteRate = header.sampleRate * header.blockAlign;
        
        // 计算数据大小：帧数 * 每帧字节数
        size_t bytesPerFrame = header.channels * header.bitsPerSample / 8;
        header.dataSize = frames * bytesPerFrame;
        header.size = header.dataSize + sizeof(WavHeader) - 8;

        // 写入头
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        // 转换为 16-bit PCM 并写入
        std::vector<int16_t> pcmData(frames * static_cast<int>(config_.channels));
        convertToInt16(input, pcmData.data(), frames);
        
        // 验证数据大小
        if (pcmData.size() * sizeof(int16_t) != header.dataSize) {
            std::cerr << "Warning: Data size mismatch. Expected " << header.dataSize 
                      << " bytes, but got " << (pcmData.size() * sizeof(int16_t)) << " bytes" << std::endl;
        }
        
        file.write(reinterpret_cast<const char*>(pcmData.data()), pcmData.size() * sizeof(int16_t));

        return true;
    }

    bool readWav(const std::string& filename, std::vector<float>& output, size_t& frames) {
        std::ifstream file(filename, std::ios::binary);
        if (!file) return false;

        // 读取 WAV 头
        WavHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        // 验证 WAV 格式
        if (memcmp(header.riff, "RIFF", 4) != 0 ||
            memcmp(header.wave, "WAVE", 4) != 0 ||
            memcmp(header.fmt, "fmt ", 4) != 0 ||
            memcmp(header.data, "data", 4) != 0 ||
            header.format != 1) {
            return false;
        }

        // 读取数据
        std::vector<int16_t> pcmData(header.dataSize / sizeof(int16_t));
        file.read(reinterpret_cast<char*>(pcmData.data()), header.dataSize);

        // 转换为 float
        frames = pcmData.size() / header.channels;
        output.resize(pcmData.size());
        for (size_t i = 0; i < pcmData.size(); ++i) {
            output[i] = pcmData[i] / 32768.0f;
        }

        return true;
    }

    AudioConfig getProcessedConfig() const {
        return config_;
    }

private:
    void cleanup() {
        if (opusEncoder_) {
            opus_encoder_destroy(opusEncoder_);
            opusEncoder_ = nullptr;
        }
        if (opusDecoder_) {
            opus_decoder_destroy(opusDecoder_);
            opusDecoder_ = nullptr;
        }
        if (swrCtx_) {
            swr_free(&swrCtx_);
            swrCtx_ = nullptr;
        }
    }

    void convertToInt16(const void* input, int16_t* output, size_t frames) {
        const float* floatInput = static_cast<const float*>(input);
        for (size_t i = 0; i < frames * static_cast<int>(config_.channels); ++i) {
            float sample = floatInput[i];
            // 限制在 [-1.0, 1.0] 范围内
            sample = std::max(-1.0f, std::min(1.0f, sample));
            // 转换为 16-bit
            output[i] = static_cast<int16_t>(sample * 32767.0f);
        }
    }

    OpusEncoder* opusEncoder_;
    OpusDecoder* opusDecoder_;
    SwrContext* swrCtx_;
    AudioConfig config_;
};

// AudioProcessor implementation
AudioProcessor::AudioProcessor() : impl_(std::make_unique<Impl>()) {}
AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::initialize(const AudioConfig& config) { return impl_->initialize(config); }
bool AudioProcessor::startResampling(SampleRate fromRate, SampleRate toRate) {
    return impl_->startResampling(fromRate, toRate);
}
bool AudioProcessor::processResampling(const void* input, size_t inputFrames,
                                     void* output, size_t& outputFrames) {
    return impl_->processResampling(input, inputFrames, output, outputFrames);
}
void AudioProcessor::stopResampling() { impl_->stopResampling(); }
bool AudioProcessor::encodeOpus(const void* input, size_t frames, std::vector<uint8_t>& output) {
    return impl_->encodeOpus(input, frames, output);
}
bool AudioProcessor::decodeOpus(const std::vector<uint8_t>& input, void* output, size_t& outputFrames) {
    return impl_->decodeOpus(input, output, outputFrames);
}
bool AudioProcessor::writeWav(const void* input, size_t frames, const std::string& filename) {
    return impl_->writeWav(input, frames, filename);
}
bool AudioProcessor::readWav(const std::string& filename, std::vector<float>& output, size_t& frames) {
    return impl_->readWav(filename, output, frames);
}
AudioConfig AudioProcessor::getProcessedConfig() const { return impl_->getProcessedConfig(); }
bool AudioProcessor::resample(const void* input, size_t inputFrames,
                            void* output, size_t outputFrames,
                            SampleRate fromRate, SampleRate toRate) {
    return impl_->resample(input, inputFrames, output, outputFrames, fromRate, toRate);
}

} // namespace audio
} // namespace perfx 