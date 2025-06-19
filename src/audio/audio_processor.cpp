/**
 * @file audio_processor.cpp
 * @brief 音频处理器实现，包括音频编码、解码功能
 */

#include "audio/audio_processor.h"
#include "audio/audio_types.h"
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <mutex>
#include <opus/opus.h>
#include <opus/opus_types.h>
#include <opus/opus_defines.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <ogg/ogg.h>

namespace perfx {
namespace audio {

class AudioProcessor::Impl {
private:
    // 成员变量声明
    OpusEncoder* opusEncoder_;
    OpusDecoder* opusDecoder_;
    EncodingFormat encodingFormat_;
    int opusFrameLength_;
    size_t samplesPerFrame_;
    bool initialized_;
    AudioConfig config_;
    int kRequiredFrames;
    std::vector<int16_t> frameBuffer_;
    size_t bufferSize_;

public:
    Impl() : opusEncoder_(nullptr), opusDecoder_(nullptr), initialized_(false), bufferSize_(0) {
        encodingFormat_ = EncodingFormat::WAV;
        opusFrameLength_ = 20;
        kRequiredFrames = 480;  // 默认值
        samplesPerFrame_ = 960; // 默认值，后续初始化时会设置
    }

    ~Impl() {
        cleanup();
    }

    void cleanup() {
        if (opusEncoder_) {
            opus_encoder_destroy(opusEncoder_);
            opusEncoder_ = nullptr;
        }
        if (opusDecoder_) {
            opus_decoder_destroy(opusDecoder_);
            opusDecoder_ = nullptr;
        }
        initialized_ = false;
        frameBuffer_.clear();
        bufferSize_ = 0;
    }

    const AudioConfig& getConfig() const {
        return config_;
    }
    void updateConfig(const AudioConfig& config) {
        config_ = config;
    }

    bool initialize(const AudioConfig& config) {
        if (initialized_) {
            std::cerr << "AudioProcessor already initialized" << std::endl;
            return false;
        }
        config_ = config;
        encodingFormat_ = config.encodingFormat;
        opusFrameLength_ = config.opusFrameLength;
        samplesPerFrame_ = (opusFrameLength_ * static_cast<int>(config_.sampleRate)) / 1000;
        std::cout << "[DEBUG] AudioProcessor::initialize: Opus frame size = " << samplesPerFrame_ 
                  << " samples (" << config.opusFrameLength << "ms @ " 
                  << static_cast<int>(config.sampleRate) << "Hz)" << std::endl;
        if (encodingFormat_ == EncodingFormat::OPUS) {
            int error;
            opusEncoder_ = opus_encoder_create(
                static_cast<int>(config_.sampleRate),
                static_cast<int>(config_.channels),
                OPUS_APPLICATION_AUDIO,  // 改为 AUDIO 应用类型，更适合通用音频
                &error
            );
            if (error != OPUS_OK || !opusEncoder_) {
                std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                return false;
            }
            opus_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(config_.opusBitrate));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_COMPLEXITY(config_.opusComplexity));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_DTX(0));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_INBAND_FEC(1));  // 启用前向纠错
            opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(5));  // 设置5%的丢包率容忍
            opus_encoder_ctl(opusEncoder_, OPUS_SET_LSB_DEPTH(16));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR(1));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR_CONSTRAINT(0));
            AUDIO_LOG("Opus encoder initialized");
        }
        if (config.framesPerBuffer <= 0) {
            std::cerr << "Invalid frames per buffer: " << config.framesPerBuffer << std::endl;
            return false;
        }
        if (config.inputDevice.index < 0) {
            std::cerr << "Invalid input device configuration" << std::endl;
            return false;
        }
        if (config.encodingFormat == EncodingFormat::OPUS) {
            if (config.opusFrameLength <= 0 || config.opusBitrate <= 0 || 
                config.opusComplexity < 0 || config.opusComplexity > 10) {
                std::cerr << "Invalid Opus encoding parameters" << std::endl;
                return false;
            }
        }
        // DEBUG: 验证保存的配置
        std::cout << "[DEBUG] AudioProcessor::initialize config:" << std::endl;
        std::cout << "  - sampleRate: " << static_cast<int>(config_.sampleRate) << std::endl;
        std::cout << "  - channels: " << static_cast<int>(config_.channels) << std::endl;
        std::cout << "  - format: " << static_cast<int>(config_.format) << " (INT16=0, FLOAT32=3)" << std::endl;
        initialized_ = true;
        return true;
    }

    void processAudio(const void* input, void* output, unsigned long frameCount) {
        if (!input || !output || frameCount == 0) return;

        if (config_.format == SampleFormat::FLOAT32) {
            // 处理 FLOAT32 格式
            const float* floatInput = static_cast<const float*>(input);
            float* floatOutput = static_cast<float*>(output);
            std::memcpy(floatOutput, floatInput, frameCount * sizeof(float) * static_cast<int>(config_.channels));
        } else {
            // 处理 INT16 格式
            const int16_t* intInput = static_cast<const int16_t*>(input);
            int16_t* intOutput = static_cast<int16_t*>(output);
            std::memcpy(intOutput, intInput, frameCount * sizeof(int16_t) * static_cast<int>(config_.channels));
        }
    }

    bool encodeOpus(const void* input, size_t frames, std::vector<std::vector<uint8_t>>& encodedFrames) {
        encodedFrames.clear();
        if (!opusEncoder_ || !input || frames == 0) return false;

        int channels = static_cast<int>(config_.channels);
        int maxFrameSize = 4000; // Opus 官方推荐最大字节数

        std::cout << "[DEBUG] Opus encoding: frame size = " << samplesPerFrame_ 
                  << " samples, input frames = " << frames << std::endl;

        // 将新数据添加到缓冲区，同时处理格式转换
        size_t newSamples = frames * channels;
        if (config_.format == SampleFormat::FLOAT32) {
            // 处理 FLOAT32 格式
            const float* floatInput = static_cast<const float*>(input);
            std::vector<int16_t> convertedSamples(newSamples);
            
            // 转换 FLOAT32 到 INT16
            for (size_t i = 0; i < newSamples; ++i) {
                float sample = floatInput[i];
                // 处理无效值
                if (std::isnan(sample) || std::isinf(sample)) {
                    sample = 0.0f;
                }
                // 确保样本在 [-1.0, 1.0] 范围内
                sample = std::max(-1.0f, std::min(1.0f, sample));
                // 转换为 INT16
                convertedSamples[i] = static_cast<int16_t>(std::round(sample * 32767.0f));
            }
            
            // 添加到缓冲区
            frameBuffer_.insert(frameBuffer_.end(), convertedSamples.begin(), convertedSamples.end());
        } else {
            // 直接处理 INT16 格式
            const int16_t* pcm = static_cast<const int16_t*>(input);
            frameBuffer_.insert(frameBuffer_.end(), pcm, pcm + newSamples);
        }
        
        bufferSize_ += frames;

        // 当缓冲区中有足够的数据时进行编码
        while (bufferSize_ >= samplesPerFrame_) {
            std::vector<uint8_t> frameBuf(maxFrameSize);
            int encodedBytes = opus_encode(opusEncoder_,
                                         frameBuffer_.data(),
                                         static_cast<int>(samplesPerFrame_),
                                         frameBuf.data(),
                                         maxFrameSize);
            if (encodedBytes < 0) {
                std::cerr << "Opus encode error: " << opus_strerror(encodedBytes) << std::endl;
                return false;
            }
            frameBuf.resize(encodedBytes);
            encodedFrames.push_back(std::move(frameBuf));

            // 移除已编码的数据
            frameBuffer_.erase(frameBuffer_.begin(), frameBuffer_.begin() + samplesPerFrame_ * channels);
            bufferSize_ -= samplesPerFrame_;
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
                AUDIO_LOG("Initializing Opus encoder for new format");
                int error;
                opusEncoder_ = opus_encoder_create(
                    static_cast<int>(config_.sampleRate),
                    static_cast<int>(config_.channels),
                    OPUS_APPLICATION_AUDIO,  // 改为 AUDIO 应用类型，更适合通用音频
                    &error
                );
                if (error != OPUS_OK || !opusEncoder_) {
                    std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                    return;
                }

                // 使用配置中的参数设置Opus编码器
                opus_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(config_.opusBitrate));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_COMPLEXITY(config_.opusComplexity));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_LSB_DEPTH(16));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_DTX(0));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_INBAND_FEC(1));  // 启用前向纠错
                opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(5));  // 设置5%的丢包率容忍

                // 更新帧大小
                samplesPerFrame_ = (config_.opusFrameLength * static_cast<int>(config_.sampleRate)) / 1000;
                std::cout << "[DEBUG] Updated Opus frame size to " << samplesPerFrame_ 
                          << " samples (" << config_.opusFrameLength << "ms @ " 
                          << static_cast<int>(config_.sampleRate) << "Hz)" << std::endl;

                AUDIO_LOG("Opus encoder initialized for new format");
            }
        }
    }

    void setOpusFrameLength(int frameLength) {
        opusFrameLength_ = frameLength;
    }

    bool isIdle() const {
        return false; // 或根据实际需求实现
    }
};

// AudioProcessor implementation
AudioProcessor::AudioProcessor() : impl_(std::make_unique<Impl>()) {}
AudioProcessor::~AudioProcessor() = default;

bool AudioProcessor::initialize(const AudioConfig& config) {
    return impl_->initialize(config);
}

const AudioConfig& AudioProcessor::getConfig() const {
    return impl_->getConfig();
}

void AudioProcessor::updateConfig(const AudioConfig& config) {
    config_ = config;
    impl_->updateConfig(config);  // 同时更新内部实现的配置
}

void AudioProcessor::processAudio(const void* input, void* output, unsigned long frameCount) {
    if (!input || !output || frameCount == 0) return;

    if (config_.format == SampleFormat::FLOAT32) {
        // 处理 FLOAT32 格式
        const float* floatInput = static_cast<const float*>(input);
        float* floatOutput = static_cast<float*>(output);
        std::memcpy(floatOutput, floatInput, frameCount * sizeof(float) * static_cast<int>(config_.channels));
    } else {
        // 处理 INT16 格式
        const int16_t* intInput = static_cast<const int16_t*>(input);
        int16_t* intOutput = static_cast<int16_t*>(output);
        std::memcpy(intOutput, intInput, frameCount * sizeof(int16_t) * static_cast<int>(config_.channels));
    }
}

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

bool AudioProcessor::decodeOpus(const std::vector<uint8_t>& input, void* output, size_t& outputFrames) {
    return impl_->decodeOpus(input, output, outputFrames);
}

void AudioProcessor::setEncodingFormat(EncodingFormat format) {
    impl_->setEncodingFormat(format);
}

void AudioProcessor::setOpusFrameLength(int frameLength) {
    impl_->setOpusFrameLength(frameLength);
}

} // namespace audio
} // namespace perfx 