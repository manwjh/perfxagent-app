/**
 * @file audio_processor.cpp
 * @brief 音频处理器实现，包括音频编码、解码功能
 */

#include "../../include/audio/audio_processor.h"
#include "../../include/audio/audio_types.h"
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <iostream>
#include <chrono>
#include <sstream>
#include <mutex>
extern "C" {
#include <rnnoise.h>
#include <opus/opus.h>
#include <opus/opus_types.h>
#include <opus/opus_defines.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <ogg/ogg.h>
}

namespace perfx {
namespace audio {

class AudioProcessor::Impl {
public:
    Impl() : opusEncoder_(nullptr), opusDecoder_(nullptr), rnn_(nullptr) {
        encodingFormat_ = EncodingFormat::WAV;
        opusFrameLength_ = 20;
        vadConfig_.threshold = 0.5f;  // 默认 VAD 阈值
        kRequiredFrames = 480;  // 默认值，将在初始化时更新
    }

    ~Impl() {
        cleanup();
    }

    bool initialize(const AudioConfig& config) {
        if (initialized_) {
            std::cerr << "AudioProcessor already initialized" << std::endl;
            return false;
        }

        config_ = config;
        encodingFormat_ = config.encodingFormat;
        opusFrameLength_ = config.opusFrameLength;

        // 计算完整的 Opus 帧大小
        samplesPerFrame_ = (opusFrameLength_ * static_cast<int>(config_.sampleRate)) / 1000;
        std::cout << "[DEBUG] AudioProcessor::initialize: Opus frame size = " << samplesPerFrame_ 
                  << " samples (" << config.opusFrameLength << "ms @ " 
                  << static_cast<int>(config.sampleRate) << "Hz)" << std::endl;

        // 初始化 Opus 编码器
        if (encodingFormat_ == EncodingFormat::OPUS) {
            int error;
            opusEncoder_ = opus_encoder_create(
                static_cast<int>(config_.sampleRate),
                static_cast<int>(config_.channels),
                OPUS_APPLICATION_VOIP,  // 使用默认的 VOIP 应用类型
                &error
            );
            if (error != OPUS_OK || !opusEncoder_) {
                std::cerr << "Failed to create Opus encoder: " << opus_strerror(error) << std::endl;
                return false;
            }

            // 设置 Opus 编码器参数
            opus_encoder_ctl(opusEncoder_, OPUS_SET_BITRATE(config_.opusBitrate));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_COMPLEXITY(config_.opusComplexity));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_DTX(1));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_FORCE_CHANNELS(static_cast<int>(config_.channels)));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(5));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_LSB_DEPTH(16));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR(1));
            opus_encoder_ctl(opusEncoder_, OPUS_SET_VBR_CONSTRAINT(0));

            AUDIO_LOG("Opus encoder initialized");
        }

        // 验证配置参数的有效性
        if (config.framesPerBuffer <= 0) {
            std::cerr << "Invalid frames per buffer: " << config.framesPerBuffer << std::endl;
            return false;
        }

        // 验证设备配置
        if (config.inputDevice.index < 0) {
            std::cerr << "Invalid input device configuration" << std::endl;
            return false;
        }
        
        // 验证 Opus 相关参数
        if (config.encodingFormat == EncodingFormat::OPUS) {
            if (config.opusFrameLength <= 0 || config.opusBitrate <= 0 || 
                config.opusComplexity < 0 || config.opusComplexity > 10) {
                std::cerr << "Invalid Opus encoding parameters" << std::endl;
                return false;
            }
        }

        // 设置 RNNoise 参数
        kRequiredFrames = 480;  // RNNoise 需要 480 采样点
        if (config.enableVAD) {
            rnn_ = rnnoise_create(nullptr);
            if (!rnn_) {
                std::cerr << "Failed to create RNNoise instance" << std::endl;
                return false;
            }
        }

        // DEBUG: 验证保存的配置
        std::cout << "[DEBUG] AudioProcessor::initialize config:" << std::endl;
        std::cout << "  - sampleRate: " << static_cast<int>(config_.sampleRate) << std::endl;
        std::cout << "  - channels: " << static_cast<int>(config_.channels) << std::endl;
        std::cout << "  - format: " << static_cast<int>(config_.format) << " (INT16=0, FLOAT32=3)" << std::endl;
        std::cout << "  - enableVAD: " << config_.enableVAD << std::endl;
 
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
                    OPUS_APPLICATION_VOIP,  // 使用默认的 VOIP 应用类型
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
                opus_encoder_ctl(opusEncoder_, OPUS_SET_INBAND_FEC(0));
                opus_encoder_ctl(opusEncoder_, OPUS_SET_PACKET_LOSS_PERC(0));

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

    bool initializeVAD(const VADConfig& config) {
        vadConfig_ = config;
        if (!config.enabled) {
            if (rnn_) {
                rnnoise_destroy(rnn_);
                rnn_ = nullptr;
            }
            return true;
        }

        if (!rnn_) {
            rnn_ = rnnoise_create(nullptr);
            if (!rnn_) {
                std::cerr << "Failed to create RNNoise instance" << std::endl;
                return false;
            }
        }

        // 重置 VAD 状态
        vadStatus_ = VADStatus();
        return true;
    }

    // INT16 版本的 isVoiceActive 方法
    bool isVoiceActive(const int16_t* input, size_t frameCount) {
        if (!input || frameCount == 0 || !rnn_) {
            std::cerr << "[ERROR] Invalid input parameters or RNNoise not initialized" << std::endl;
            return false;
        }

        // 将 INT16 数据转换为 float 格式
        std::vector<float> floatData(frameCount);
        for (size_t i = 0; i < frameCount; ++i) {
            // 将 INT16 范围 [-32768, 32767] 转换为 float 范围 [-1.0, 1.0]
            floatData[i] = static_cast<float>(input[i]) / 32768.0f;
        }

        // 调用 float 版本的处理方法
        return isVoiceActive(floatData.data(), frameCount);
    }

    bool isVoiceActive(const float* input, size_t frameCount) {
        if (!input || frameCount == 0 || !rnn_) {
            std::cerr << "[ERROR] Invalid input parameters or RNNoise not initialized" << std::endl;
            return false;
        }

        // 检查RNNoise实例
        std::cout << "[VAD] RNNoise instance check: " << (rnn_ ? "valid" : "null") << std::endl;

        // 检查输入数据
        std::cout << "[VAD] Processing float32 input data" << std::endl;
        std::cout << "[VAD] Input frame count: " << frameCount << std::endl;
        
        // 打印前10个样本值用于调试
        std::cout << "[VAD] First 10 float32 samples: ";
        for (size_t i = 0; i < std::min(size_t(10), frameCount); ++i) {
            std::cout << input[i] << ", ";
        }
        std::cout << std::endl;

        // 数据验证和归一化
        std::vector<float> normalizedData(frameCount);
        float maxValue = -std::numeric_limits<float>::infinity();
        float minValue = std::numeric_limits<float>::infinity();
        size_t validSamples = 0;
        size_t nonZeroSamples = 0;

        for (size_t i = 0; i < frameCount; ++i) {
            float sample = input[i];
            
            // 检查是否为有效数值
            if (!std::isfinite(sample)) {
                std::cerr << "[ERROR] Invalid float32 data at index " << i 
                         << ": " << sample << std::endl;
                continue;
            }

            // 更新最大最小值
            maxValue = std::max(maxValue, sample);
            minValue = std::min(minValue, sample);

            // 归一化到[-1.0, 1.0]范围
            float normalizedSample = std::max(-1.0f, std::min(1.0f, sample));
            normalizedData[validSamples++] = normalizedSample;

            // 统计非零样本
            if (std::abs(normalizedSample) > 1e-6f) {
                nonZeroSamples++;
            }
        }

        // 打印数据统计信息
        std::cout << "[VAD] Input data range: min=" << minValue << ", max=" << maxValue << std::endl;
        std::cout << "[VAD] Non-zero samples: " << nonZeroSamples << "/" << frameCount 
                 << " (" << (nonZeroSamples * 100.0f / frameCount) << "%)" << std::endl;

        // 检查是否有足够的有效样本
        if (validSamples < frameCount) {
            std::cerr << "[ERROR] Insufficient valid samples: " << validSamples 
                     << "/" << frameCount << std::endl;
            return false;
        }

        // 将数据添加到缓冲区
        std::cout << "[VAD] Before buffer insertion - Buffer size: " << bufferSize_ 
                 << ", Frame buffer size: " << frameBuffer_.size() << std::endl;
        
        frameBuffer_.insert(frameBuffer_.end(), normalizedData.begin(), normalizedData.end());
        bufferSize_ = frameBuffer_.size();  // 确保 bufferSize_ 与 frameBuffer_.size() 同步
        
        std::cout << "[VAD] After buffer insertion - Buffer size: " << bufferSize_ 
                 << ", Frame buffer size: " << frameBuffer_.size() << std::endl;

        std::cout << "[VAD] Buffer status: " << bufferSize_ << "/" << kRequiredFrames 
                 << " frames (input: " << frameCount << " frames)" << std::endl;

        // 如果缓冲区数据不足，返回false
        if (bufferSize_ < kRequiredFrames) {
            std::cout << "[VAD] Insufficient frames, waiting for more data" << std::endl;
            return false;
        }

        // 处理完整的帧
        float maxVoiceProb = 0.0f;
        size_t processedFrames = 0;

        // 处理所有完整的帧
        while (bufferSize_ >= kRequiredFrames) {
            std::cout << "[VAD] Processing frame " << (processedFrames + 1) 
                     << " with " << bufferSize_ << " frames in buffer" << std::endl;

            // 创建临时缓冲区用于 RNNoise 处理
            std::vector<float> frameData(kRequiredFrames);
            
            // 复制数据到临时缓冲区
            std::cout << "[VAD] Copying " << kRequiredFrames << " frames to temporary buffer" << std::endl;
            std::copy(frameBuffer_.begin(), 
                     frameBuffer_.begin() + kRequiredFrames,
                     frameData.begin());

            // 打印第一帧的数据范围
            if (processedFrames == 0) {
                float frameMin = *std::min_element(frameData.begin(), frameData.end());
                float frameMax = *std::max_element(frameData.begin(), frameData.end());
                std::cout << "[VAD] First frame to RNNoise: min=" << frameMin 
                         << ", max=" << frameMax << std::endl;
            }

            // 检查帧数据是否有效
            bool frameDataValid = true;
            for (size_t i = 0; i < frameData.size(); ++i) {
                if (!std::isfinite(frameData[i])) {
                    std::cerr << "[ERROR] Invalid frame data at index " << i 
                             << ": " << frameData[i] << std::endl;
                    frameDataValid = false;
                    break;
                }
            }

            if (!frameDataValid) {
                std::cerr << "[ERROR] Invalid frame data detected" << std::endl;
                return false;
            }

            // 检查RNNoise实例和输入数据
            std::cout << "[VAD] RNNoise instance before processing: " << (rnn_ ? "valid" : "null") << std::endl;
            std::cout << "[VAD] Frame data pointer: " << frameData.data() << std::endl;
            std::cout << "[VAD] Frame data size: " << frameData.size() << std::endl;

            // 使用 RNNoise 处理数据
            std::cout << "[VAD] About to call rnnoise_process_frame" << std::endl;
            
            // 检查 RNNoise 实例的有效性
            if (!rnn_) {
                std::cerr << "[ERROR] RNNoise instance is null" << std::endl;
                return false;
            }

            // 检查 RNNoise 帧大小
            int rnnoiseFrameSize = rnnoise_get_frame_size();
            std::cout << "[DEBUG] RNNoise frame size check:" << std::endl;
            std::cout << "- RNNoise frame size: " << rnnoiseFrameSize << std::endl;
            std::cout << "- Required frames: " << kRequiredFrames << std::endl;
            std::cout << "- Frame size match: " << (static_cast<size_t>(rnnoiseFrameSize) == kRequiredFrames ? "yes" : "no") << std::endl;

            if (static_cast<size_t>(rnnoiseFrameSize) != kRequiredFrames) {
                std::cerr << "[ERROR] RNNoise frame size mismatch" << std::endl;
                return false;
            }

            // 检查内存访问权限
            if (!frameData.data()) {
                std::cerr << "[ERROR] Frame data pointer is null" << std::endl;
                return false;
            }

            // 检查内存是否可写
            try {
                frameData[0] = frameData[0];  // 尝试写入第一个元素
            } catch (...) {
                std::cerr << "[ERROR] Frame data is not writable" << std::endl;
                return false;
            }

            // 检查内存是否对齐
            uintptr_t dataAddr = reinterpret_cast<uintptr_t>(frameData.data());
            std::cout << "[DEBUG] Memory alignment check:" << std::endl;
            std::cout << "- Data address: 0x" << std::hex << dataAddr << std::dec << std::endl;
            std::cout << "- Alignment offset: " << (dataAddr % 16) << " bytes" << std::endl;
            std::cout << "- Is 16-byte aligned: " << (dataAddr % 16 == 0 ? "yes" : "no") << std::endl;

            if (dataAddr % 16 != 0) {
                std::cerr << "[ERROR] Frame data is not 16-byte aligned" << std::endl;
                return false;
            }

            // 检查数据范围
            std::cout << "[DEBUG] Checking data range:" << std::endl;
            bool dataInRange = true;
            float minVal = std::numeric_limits<float>::max();
            float maxVal = std::numeric_limits<float>::lowest();
            size_t invalidCount = 0;
            size_t outOfRangeCount = 0;

            for (size_t i = 0; i < frameData.size(); ++i) {
                float val = frameData[i];
                if (!std::isfinite(val)) {
                    std::cerr << "[ERROR] Invalid value at index " << i 
                             << ": " << val << std::endl;
                    invalidCount++;
                    continue;
                }
                minVal = std::min(minVal, val);
                maxVal = std::max(maxVal, val);
                if (val < -1.0f || val > 1.0f) {
                    std::cerr << "[ERROR] Data out of range at index " << i 
                             << ": " << val << std::endl;
                    outOfRangeCount++;
                    dataInRange = false;
                }
            }

            std::cout << "[DEBUG] Data range check results:" << std::endl;
            std::cout << "- Min value: " << minVal << std::endl;
            std::cout << "- Max value: " << maxVal << std::endl;
            std::cout << "- Invalid values: " << invalidCount << std::endl;
            std::cout << "- Out of range values: " << outOfRangeCount << std::endl;

            if (!dataInRange || invalidCount > 0) {
                std::cerr << "[ERROR] Invalid data range detected" << std::endl;
                return false;
            }

            // 检查 RNNoise 实例状态
            std::cout << "[DEBUG] RNNoise instance state:" << std::endl;
            std::cout << "- Instance pointer: " << static_cast<void*>(rnn_) << std::endl;
            std::cout << "- Frame size: " << rnnoiseFrameSize << std::endl;
            std::cout << "- Data pointer: " << static_cast<void*>(frameData.data()) << std::endl;
            std::cout << "- Data size: " << frameData.size() << std::endl;

            // 创建输出缓冲区用于降噪处理
            std::vector<float> outputData(kRequiredFrames);
            
            // 调用 RNNoise 处理
            std::cout << "[DEBUG] Calling rnnoise_process_frame..." << std::endl;
            float voiceProb = rnnoise_process_frame(rnn_, outputData.data(), frameData.data());
            std::cout << "[DEBUG] Successfully returned from rnnoise_process_frame" << std::endl;
            std::cout << "[VAD] RNNoise returned voice probability: " << voiceProb << std::endl;
            
            if (!std::isfinite(voiceProb) || voiceProb < 0.0f || voiceProb > 1.0f) {
                std::cerr << "[ERROR] Invalid voice probability: " << voiceProb << std::endl;
                return false;
            }

            maxVoiceProb = std::max(maxVoiceProb, voiceProb);

            // 移除已处理的数据，保留剩余帧
            size_t remainingFrames = bufferSize_ - kRequiredFrames;
            std::cout << "[VAD] After processing - Remaining frames: " << remainingFrames << std::endl;
            
            if (remainingFrames > 0) {
                // 创建临时缓冲区存储剩余帧
                std::vector<float> tempBuffer(remainingFrames);
                std::cout << "[VAD] Creating temporary buffer for " << remainingFrames << " frames" << std::endl;
                
                std::copy(frameBuffer_.begin() + kRequiredFrames,
                         frameBuffer_.end(),
                         tempBuffer.begin());
                
                // 清空并重新填充主缓冲区
                std::cout << "[VAD] Clearing and refilling main buffer" << std::endl;
                frameBuffer_.clear();
                frameBuffer_.insert(frameBuffer_.end(), tempBuffer.begin(), tempBuffer.end());
                bufferSize_ = remainingFrames;
                
                std::cout << "[VAD] Kept " << remainingFrames << " frames for next processing" << std::endl;

                // 如果剩余帧数超过设备帧大小，只保留最后256帧
                if (remainingFrames > frameCount) {
                    std::cout << "[VAD] Truncating buffer from " << remainingFrames 
                             << " to " << frameCount << " frames" << std::endl;
                    
                    std::vector<float> lastFrames(frameCount);
                    std::copy(frameBuffer_.end() - frameCount,
                             frameBuffer_.end(),
                             lastFrames.begin());
                    frameBuffer_.clear();
                    frameBuffer_.insert(frameBuffer_.end(), lastFrames.begin(), lastFrames.end());
                    bufferSize_ = frameCount;
                    std::cout << "[VAD] Buffer truncated to " << frameCount << " frames" << std::endl;
                }
            } else {
                std::cout << "[VAD] No remaining frames, clearing buffer" << std::endl;
                frameBuffer_.clear();
                bufferSize_ = 0;
            }
            processedFrames++;
        }

        // 更新VAD状态
        bool isVoice = maxVoiceProb > vadConfig_.threshold;
        updateVADStatus(isVoice);

        // 打印VAD状态
        std::cout << "[VAD] Processed " << processedFrames << " frames, "
                 << "Max voice probability: " << maxVoiceProb 
                 << ", Threshold: " << vadConfig_.threshold << std::endl;

        return isVoice;
    }

    void updateVADStatus(bool isVoiceActive) {
        int64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        VADState previousState = vadStatus_.state;  // 保存之前的状态

        if (isVoiceActive) {
            vadStatus_.isVoiceActive = true;
            vadStatus_.lastVoiceTime = currentTime;
            vadStatus_.currentSilenceDuration = 0;
            vadStatus_.silenceFrameCount = 0;
            vadStatus_.voiceFrameCount++;
            vadStatus_.state = VADState::SPEAKING;
            
            if (previousState != VADState::SPEAKING) {
                AUDIO_LOG("VAD State Change: " << getVADStateString(previousState) << " -> SPEAKING");
                AUDIO_LOG("VAD Details: voice probability = " << vadStatus_.voiceProbability 
                         << ", threshold = " << vadConfig_.threshold
                         << ", voice frame count = " << vadStatus_.voiceFrameCount);
            }
        } else {
            vadStatus_.isVoiceActive = false;
            vadStatus_.currentSilenceDuration = currentTime - vadStatus_.lastVoiceTime;
            vadStatus_.silenceFrameCount++;
            vadStatus_.voiceFrameCount = 0;

            // 更新状态
            if (vadConfig_.enableIdleDetection && 
                vadStatus_.currentSilenceDuration > vadConfig_.silenceTimeoutMs) {
                vadStatus_.state = VADState::IDLE;
                if (previousState != VADState::IDLE) {
                    AUDIO_LOG("VAD State Change: " << getVADStateString(previousState) << " -> IDLE");
                    AUDIO_LOG("VAD Details: silence duration = " << vadStatus_.currentSilenceDuration 
                             << "ms, timeout = " << vadConfig_.silenceTimeoutMs << "ms");
                }
            } else if (vadConfig_.enableSentenceDetection && 
                      vadStatus_.currentSilenceDuration > vadConfig_.sentenceTimeoutMs) {
                vadStatus_.state = VADState::SENTENCE_END;
                if (previousState != VADState::SENTENCE_END) {
                    AUDIO_LOG("VAD State Change: " << getVADStateString(previousState) << " -> SENTENCE_END");
                    AUDIO_LOG("VAD Details: silence duration = " << vadStatus_.currentSilenceDuration 
                             << "ms, timeout = " << vadConfig_.sentenceTimeoutMs << "ms");
                }
            } else {
                vadStatus_.state = VADState::SILENCE;
                if (previousState != VADState::SILENCE) {
                    AUDIO_LOG("VAD State Change: " << getVADStateString(previousState) << " -> SILENCE");
                    AUDIO_LOG("VAD Details: silence frame count = " << vadStatus_.silenceFrameCount 
                             << ", silence duration = " << vadStatus_.currentSilenceDuration << "ms");
                }
            }
        }
    }

    // Helper function to convert VADState to string
    std::string getVADStateString(VADState state) {
        switch (state) {
            case VADState::IDLE:
                return "IDLE";
            case VADState::SILENCE:
                return "SILENCE";
            case VADState::SPEAKING:
                return "SPEAKING";
            case VADState::SENTENCE_END:
                return "SENTENCE_END";
            default:
                return "UNKNOWN";
        }
    }

    void setVADThreshold(float threshold) {
        if (threshold >= 0.0f && threshold <= 1.0f) {
            vadConfig_.threshold = threshold;
        }
    }

    float getVADThreshold() const {
        return vadConfig_.threshold;
    }

    const VADStatus& getVADStatus() const {
        return vadStatus_;
    }

    void updateVADConfig(const VADConfig& config) {
        vadConfig_ = config;
    }

    const VADConfig& getVADConfig() const {
        return vadConfig_;
    }

    bool shouldInsertSilenceFrame() const {
        return vadConfig_.enabled && 
               vadConfig_.enableSilenceFrame && 
               !vadStatus_.isVoiceActive;
    }

    bool isSentenceEnd() const {
        return vadConfig_.enabled && 
               vadConfig_.enableSentenceDetection && 
               vadStatus_.state == VADState::SENTENCE_END;
    }

    bool isIdle() const {
        return vadConfig_.enabled && 
               vadConfig_.enableIdleDetection && 
               vadStatus_.state == VADState::IDLE;
    }

    void updateConfig(const AudioConfig& config) {
        config_ = config;
    }

    const AudioConfig& getConfig() const {
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
        if (rnn_) {
            rnnoise_destroy(rnn_);
            rnn_ = nullptr;
        }
    }

    void convertToInt16(const void* input, int16_t* output, size_t frames) {
        if (!input || !output || frames == 0) {
            std::cerr << "[ERROR] Invalid input parameters in convertToInt16" << std::endl;
            return;
        }

        if (config_.format == SampleFormat::FLOAT32) {
            const float* floatInput = static_cast<const float*>(input);
            size_t totalSamples = frames * static_cast<int>(config_.channels);
            
            // 数据验证和预处理
            bool dataValid = true;
            size_t validSamples = 0;
            float maxValue = -std::numeric_limits<float>::infinity();
            float minValue = std::numeric_limits<float>::infinity();
            
            // 第一次遍历：验证数据
            for (size_t i = 0; i < totalSamples; ++i) {
                float sample = floatInput[i];
                if (std::isnan(sample) || std::isinf(sample)) {
                    std::cerr << "[ERROR] Invalid float32 data at index " << i << ": " << sample << std::endl;
                    dataValid = false;
                    continue;
                }
                
                // 更新最大最小值
                maxValue = std::max(maxValue, sample);
                minValue = std::min(minValue, sample);
                validSamples++;
            }
            
            // 如果数据无效，记录警告
            if (!dataValid) {
                std::cerr << "[WARNING] Invalid data detected in input buffer" << std::endl;
                std::cerr << "Valid samples: " << validSamples << "/" << totalSamples << std::endl;
                std::cerr << "Data range: [" << minValue << ", " << maxValue << "]" << std::endl;
            }
            
            // 第二次遍历：转换数据
            for (size_t i = 0; i < totalSamples; ++i) {
                float sample = floatInput[i];
                // 处理无效值
                if (std::isnan(sample) || std::isinf(sample)) {
                    sample = 0.0f;
                }
                // 确保样本在 [-1.0, 1.0] 范围内
                sample = std::max(-1.0f, std::min(1.0f, sample));
                output[i] = static_cast<int16_t>(std::round(sample * 32767.0f));
            }
        } else {
            throw std::runtime_error("Unsupported sample format for conversion to INT16");
        }
    }

    AudioConfig config_;
    EncodingFormat encodingFormat_;
    int opusFrameLength_;
    VADConfig vadConfig_;
    VADStatus vadStatus_;
    OpusEncoder* opusEncoder_;
    OpusDecoder* opusDecoder_;
    DenoiseState* rnn_;
    std::vector<int16_t> frameBuffer_;  // 用于累积帧的缓冲区，始终使用 INT16 格式
    size_t bufferSize_ = 0;           // 当前缓冲区中的帧数
    size_t kRequiredFrames;           // RNNoise需要的帧数，将在初始化时设置
    size_t samplesPerFrame_;  // Opus 帧大小（采样点数）
    std::mutex mutex_;
    bool initialized_ = false;
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

bool AudioProcessor::initializeVAD(const VADConfig& config) {
    return impl_->initializeVAD(config);
}

bool AudioProcessor::isVoiceActive(const float* input, size_t frameCount) {
    return impl_->isVoiceActive(input, frameCount);
}

bool AudioProcessor::isVoiceActive(const int16_t* input, size_t frameCount) {
    return impl_->isVoiceActive(input, frameCount);
}

void AudioProcessor::setVADThreshold(float threshold) {
    impl_->setVADThreshold(threshold);
}

float AudioProcessor::getVADThreshold() const {
    return impl_->getVADThreshold();
}

const VADStatus& AudioProcessor::getVADStatus() const {
    return impl_->getVADStatus();
}

void AudioProcessor::updateVADConfig(const VADConfig& config) {
    impl_->updateVADConfig(config);
}

const VADConfig& AudioProcessor::getVADConfig() const {
    return impl_->getVADConfig();
}

bool AudioProcessor::shouldInsertSilenceFrame() const {
    return impl_->shouldInsertSilenceFrame();
}

bool AudioProcessor::isSentenceEnd() const {
    return impl_->isSentenceEnd();
}

bool AudioProcessor::isIdle() const {
    return impl_->isIdle();
}

void AudioProcessor::setEncodingFormat(EncodingFormat format) {
    impl_->setEncodingFormat(format);
}

void AudioProcessor::setOpusFrameLength(int frameLength) {
    impl_->setOpusFrameLength(frameLength);
}

} // namespace audio
} // namespace perfx 