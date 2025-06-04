/**
 * @file audio_processor.cpp
 * @brief 音频处理器实现，包括音频编码、解码和重采样功能
 */

#include "../../include/audio/audio_processor.h"
#include "../../include/audio/audio_types.h"
#include <opus/opus.h>
#include <ogg/ogg.h>
#include <sndfile.h>
extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
}
#include <samplerate.h>
// #include <webrtc/common_audio/vad/include/webrtc_vad.h>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <iostream>

namespace perfx {
namespace audio {

class AudioProcessor::Impl {
public:
    Impl() : opusEncoder_(nullptr), opusDecoder_(nullptr), swrCtx_(nullptr) {
        encodingFormat_ = EncodingFormat::WAV;
        opusFrameLength_ = 20;
    }

    ~Impl() {
        cleanup();
    }

    bool initialize(const AudioConfig& config) {
        try {
            config_ = config;
            
            // 设置默认编码格式为WAV
            encodingFormat_ = EncodingFormat::WAV;
            
            // 初始化 Opus 编码器
            int error;
            opusEncoder_ = opus_encoder_create(
                static_cast<int>(config_.sampleRate),
                static_cast<int>(config_.channels),
                OPUS_APPLICATION_VOIP,  // 使用VOIP模式，针对人声优化
                &error
            );
            if (error != OPUS_OK || !opusEncoder_) {
                std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                return false;
            }

            // 初始化 Opus 解码器
            opusDecoder_ = opus_decoder_create(
                static_cast<int>(config_.sampleRate),
                static_cast<int>(config_.channels),
                &error
            );
            if (error != OPUS_OK || !opusDecoder_) {
                std::cerr << "Failed to create Opus decoder: " << opus_strerror(error) << std::endl;
                opus_encoder_destroy(opusEncoder_);
                opusEncoder_ = nullptr;
                return false;
            }

            // 设置 Opus 编码器参数，针对人声优化
            opus_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(48000));  // 设置比特率为48kbps
            opus_encoder_ctl(opusEncoder_, OPUS_SET_COMPLEXITY(6));  // 设置复杂度为6
            opus_encoder_ctl(opusEncoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));  // 设置为语音信号
            opus_encoder_ctl(opusEncoder_, OPUS_SET_DTX(1));  // 启用DTX，节省带宽
            opus_encoder_ctl(opusEncoder_, OPUS_SET_FORCE_CHANNELS(static_cast<int>(config_.channels)));  // 强制通道数
            opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(5));  // 设置5%的丢包率，提高网络适应性
            opus_encoder_ctl(opusEncoder_, OPUS_SET_LSB_DEPTH(16));  // 设置位深度为16位
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR(1));  // 启用可变比特率
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR_CONSTRAINT(0));  // 不限制VBR

            // 验证编码器设置
            int value;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_BITRATE(&value));
            std::cout << "[DEBUG] Opus encoder initialized successfully" << std::endl;
            std::cout << "  - Sample rate: " << static_cast<int>(config_.sampleRate) << "Hz" << std::endl;
            std::cout << "  - Channels: " << static_cast<int>(config_.channels) << std::endl;
            std::cout << "  - Bitrate: " << value << "bps" << std::endl;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_COMPLEXITY(&value));
            std::cout << "  - Complexity: " << value << std::endl;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_SIGNAL(&value));
            std::cout << "  - Signal type: " << (value == OPUS_SIGNAL_VOICE ? "Voice" : "Music") << std::endl;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_DTX(&value));
            std::cout << "  - DTX: " << (value ? "Enabled" : "Disabled") << std::endl;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_VBR(&value));
            std::cout << "  - VBR: " << (value ? "Enabled" : "Disabled") << std::endl;
            opus_encoder_ctl(opusEncoder_, OPUS_GET_PACKET_LOSS_PERC(&value));
            std::cout << "  - Packet loss: " << value << "%" << std::endl;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Error in AudioProcessor::initialize: " << e.what() << std::endl;
            return false;
        }
    }

    bool startResampling(SampleRate fromRate, SampleRate toRate) {
        if (swrCtx_) {
            swr_free(&swrCtx_);
        }

        swrCtx_ = swr_alloc();
        if (!swrCtx_) {
            throw std::runtime_error("Failed to allocate resampling context");
        }

        // 设置通道布局
        int64_t in_channel_layout = (config_.channels == ChannelCount::MONO) ? 
            AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
        int64_t out_channel_layout = in_channel_layout;

        // 设置重采样参数
        av_opt_set_int(swrCtx_, "in_channel_count", static_cast<int>(config_.channels), 0);
        av_opt_set_int(swrCtx_, "out_channel_count", static_cast<int>(config_.channels), 0);
        av_opt_set_int(swrCtx_, "in_channel_layout", in_channel_layout, 0);
        av_opt_set_int(swrCtx_, "out_channel_layout", out_channel_layout, 0);
        av_opt_set_int(swrCtx_, "in_sample_rate", static_cast<int>(fromRate), 0);
        av_opt_set_int(swrCtx_, "out_sample_rate", static_cast<int>(toRate), 0);
        av_opt_set_sample_fmt(swrCtx_, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
        av_opt_set_sample_fmt(swrCtx_, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

        // 初始化重采样上下文
        int ret = swr_init(swrCtx_);
        if (ret < 0) {
            swr_free(&swrCtx_);
            swrCtx_ = nullptr;
            throw std::runtime_error("Failed to initialize resampling context");
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

    bool encodeOpus(const void* input, size_t frames, std::vector<std::vector<uint8_t>>& encodedFrames) {
        encodedFrames.clear();
        if (!opusEncoder_ || !input || frames == 0) return false;

        const int16_t* pcm = static_cast<const int16_t*>(input);
        int samplesPerFrame = static_cast<int>(config_.sampleRate) * config_.opusFrameLength / 1000;
        int channels = static_cast<int>(config_.channels);
        int maxFrameSize = 4000; // Opus 官方推荐最大字节数

        size_t totalSamples = frames * channels;
        size_t offset = 0;
        while (offset + samplesPerFrame * channels <= totalSamples) {
            std::vector<uint8_t> frameBuf(maxFrameSize);
            int encodedBytes = opus_encode(opusEncoder_,
                                         pcm + offset,
                                         samplesPerFrame,
                                         frameBuf.data(),
                                         maxFrameSize);
            if (encodedBytes < 0) {
                std::cerr << "Opus encode error: " << opus_strerror(encodedBytes) << std::endl;
                return false;
            }
            frameBuf.resize(encodedBytes);
            encodedFrames.push_back(std::move(frameBuf));
            offset += samplesPerFrame * channels;
        }

        // 处理最后不足一帧的部分（补零）
        if (offset < totalSamples) {
            std::vector<int16_t> lastFrame(samplesPerFrame * channels, 0);
            size_t remain = totalSamples - offset;
            std::copy(pcm + offset, pcm + offset + remain, lastFrame.begin());
            
            std::vector<uint8_t> frameBuf(maxFrameSize);
            int encodedBytes = opus_encode(opusEncoder_,
                                         lastFrame.data(),
                                         samplesPerFrame,
                                         frameBuf.data(),
                                         maxFrameSize);
            if (encodedBytes < 0) {
                std::cerr << "Opus encode error: " << opus_strerror(encodedBytes) << std::endl;
                return false;
            }
            frameBuf.resize(encodedBytes);
            encodedFrames.push_back(std::move(frameBuf));
        }

        return true;
    }

    bool createOpusHeader(std::vector<uint8_t>& header) {
        header.resize(19);
        
        // 设置 Opus 头标识
        memcpy(header.data(), "OpusHead", 8);
        
        // 版本号
        header[8] = 1;
        
        // 通道数
        header[9] = static_cast<uint8_t>(config_.channels);
        
        // 预跳过采样数 (16-bit)
        // 对于48kHz采样率，预跳过值通常为3840个采样点
        uint16_t preSkip = static_cast<uint16_t>(static_cast<int>(config_.sampleRate) * 0.08); // 80ms
        memcpy(header.data() + 10, &preSkip, 2);
        
        // 采样率 (32-bit)
        uint32_t sampleRate = static_cast<uint32_t>(config_.sampleRate);
        memcpy(header.data() + 12, &sampleRate, 4);
        
        // 输出增益 (16-bit)
        int16_t outputGain = 0;
        memcpy(header.data() + 16, &outputGain, 2);
        
        // 通道映射
        header[18] = 0; // 默认映射
        
        return true;
    }

    bool encapsulateOpusFrame(const std::vector<uint8_t>& encodedData, std::vector<uint8_t>& output) {
        if (encodedData.empty()) {
            return false;
        }

        // 计算帧大小
        size_t frameSize = encodedData.size();
        
        // 创建OGG包
        ogg_packet op;
        op.packet = const_cast<unsigned char*>(encodedData.data());
        op.bytes = frameSize;
        op.b_o_s = 0;
        op.e_o_s = 1;
        op.granulepos = frameSize;
        op.packetno = 0;

        // 创建OGG流
        ogg_stream_state os;
        if (ogg_stream_init(&os, rand()) != 0) {
            return false;
        }

        // 将包添加到流中
        if (ogg_stream_packetin(&os, &op) != 0) {
            ogg_stream_clear(&os);
            return false;
        }

        // 获取OGG页
        ogg_page og;
        while (ogg_stream_flush(&os, &og)) {
            // 将页头添加到输出
            output.insert(output.end(), og.header, og.header + og.header_len);
            // 将页体添加到输出
            output.insert(output.end(), og.body, og.body + og.body_len);
        }

        ogg_stream_clear(&os);
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

    AudioConfig getProcessedConfig() const {
        return config_;
    }

    void setEncodingFormat(EncodingFormat format) {
        if (encodingFormat_ != format) {
            encodingFormat_ = format;
            
            // 如果切换到Opus格式，确保编码器已初始化
            if (format == EncodingFormat::OPUS && !opusEncoder_) {
                std::cout << "[DEBUG] Initializing Opus encoder for new format" << std::endl;
                int error;
                opusEncoder_ = opus_encoder_create(
                    static_cast<int>(config_.sampleRate),
                    static_cast<int>(config_.channels),
                    OPUS_APPLICATION_VOIP,
                    &error
                );
                if (error != OPUS_OK || !opusEncoder_) {
                    std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                    return;
                }

                // 设置Opus编码器参数
                opus_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(48000));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_COMPLEXITY(6));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_DTX(1));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_FORCE_CHANNELS(static_cast<int>(config_.channels)));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(5));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_LSB_DEPTH(16));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR(1));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR_CONSTRAINT(0));

                std::cout << "[DEBUG] Opus encoder initialized for new format" << std::endl;
            }
        }
    }

    void setOpusFrameLength(int frameLength) {
        opusFrameLength_ = frameLength;
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
        if (config_.format == SampleFormat::FLOAT32) {
            const float* floatInput = static_cast<const float*>(input);
            std::cout << "[DEBUG] Converting FLOAT32 to INT16, frames: " << frames 
                      << ", channels: " << static_cast<int>(config_.channels) << std::endl;
            
            // 验证输入数据
            bool hasNonZeroInput = false;
            for (size_t i = 0; i < frames * static_cast<int>(config_.channels); ++i) {
                if (std::abs(floatInput[i]) > 1e-6f) {
                    hasNonZeroInput = true;
                    break;
                }
            }
            
            if (!hasNonZeroInput) {
                std::cerr << "[ERROR] Input audio data contains only zeros!" << std::endl;
                throw std::runtime_error("Invalid input audio data: all samples are zero");
            }
            
            // 转换为16位整数
            for (size_t i = 0; i < frames * static_cast<int>(config_.channels); ++i) {
                float sample = floatInput[i];
                // 限制在 [-1.0, 1.0] 范围内
                sample = std::max(-1.0f, std::min(1.0f, sample));
                // 转换为16位整数，使用round而不是truncate
                output[i] = static_cast<int16_t>(std::round(sample * 32767.0f));
            }
            
            // 打印前几个样本用于调试
            std::cout << "[DEBUG] First few samples after conversion:" << std::endl;
            for (size_t i = 0; i < std::min(size_t(5), frames * static_cast<int>(config_.channels)); ++i) {
                std::cout << "  Sample " << i << ": " << output[i] << std::endl;
            }
        } else if (config_.format == SampleFormat::INT16) {
            // 如果已经是INT16格式，直接复制
            std::cout << "[DEBUG] Input is already INT16 format" << std::endl;
            std::memcpy(output, input, frames * sizeof(int16_t) * static_cast<int>(config_.channels));
        } else {
            std::cerr << "[ERROR] Unsupported sample format: " << static_cast<int>(config_.format) << std::endl;
            throw std::runtime_error("Unsupported sample format for conversion to INT16");
        }
    }

    AudioConfig config_;
    EncodingFormat encodingFormat_;
    int opusFrameLength_;
    OpusEncoder* opusEncoder_;
    OpusDecoder* opusDecoder_;
    SwrContext* swrCtx_;
    std::string lastError_;
};

// AudioProcessor implementation
AudioProcessor::AudioProcessor() : impl_(std::make_unique<Impl>()) {}
AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::initialize(const AudioConfig& config) {
    config_ = config;
    return true;
}

void AudioProcessor::updateConfig(const AudioConfig& config) {
    config_ = config;
}

void AudioProcessor::processAudio(const void* input, void* output, unsigned long frameCount) {
    if (!input || !output || frameCount == 0) return;

    const int16_t* inputData = static_cast<const int16_t*>(input);
    int16_t* outputData = static_cast<int16_t*>(output);

    // 直接复制数据，不做任何处理
    std::memcpy(outputData, inputData, frameCount * sizeof(int16_t) * static_cast<int>(config_.channels));
}

bool AudioProcessor::startResampling(SampleRate fromRate, SampleRate toRate) {
    return impl_->startResampling(fromRate, toRate);
}
bool AudioProcessor::processResampling(const void* input, size_t inputFrames,
                                     void* output, size_t& outputFrames) {
    return impl_->processResampling(input, inputFrames, output, outputFrames);
}
void AudioProcessor::stopResampling() { impl_->stopResampling(); }
bool AudioProcessor::encodeOpus(const void* input, size_t frames, std::vector<std::vector<uint8_t>>& encodedFrames) {
    return impl_->encodeOpus(input, frames, encodedFrames);
}
bool AudioProcessor::createOpusHeader(std::vector<uint8_t>& header) {
    return impl_->createOpusHeader(header);
}
bool AudioProcessor::encapsulateOpusFrame(const std::vector<uint8_t>& encodedData, std::vector<uint8_t>& output) {
    return impl_->encapsulateOpusFrame(encodedData, output);
}
AudioConfig AudioProcessor::getProcessedConfig() const { return impl_->getProcessedConfig(); }
bool AudioProcessor::resample(const void* input, size_t inputFrames,
                            void* output, size_t outputFrames,
                            SampleRate fromRate, SampleRate toRate) {
    return impl_->resample(input, inputFrames, output, outputFrames, fromRate, toRate);
}

void AudioProcessor::setEncodingFormat(EncodingFormat format) {
    impl_->setEncodingFormat(format);
}

void AudioProcessor::setOpusFrameLength(int frameLength) {
    impl_->setOpusFrameLength(frameLength);
}

bool AudioProcessor::decodeOpus(const std::vector<uint8_t>& input, void* output, size_t& outputFrames) {
    return impl_->decodeOpus(input, output, outputFrames);
}

} // namespace audio
} // namespace perfx 