/**
 * @file audio_types.h
 * @brief 音频模块通用类型定义
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

// Debug configuration
#ifndef NDEBUG
    #define AUDIO_DEBUG 1
#else
    #define AUDIO_DEBUG 0
#endif

// Debug logging macros
#if AUDIO_DEBUG
    #include <iostream>
    #include <sstream>
    #define AUDIO_LOG(msg) do { \
        std::stringstream ss; \
        ss << msg; \
        std::cout << "[AUDIO] " << ss.str() << std::endl; \
    } while(0)
    #define AUDIO_LOG_VAR(var) do { \
        std::stringstream ss; \
        ss << #var << " = " << var; \
        std::cout << "[AUDIO] " << ss.str() << std::endl; \
    } while(0)
#else
    #define AUDIO_LOG(msg)
    #define AUDIO_LOG_VAR(var)
#endif

namespace perfx {
namespace audio {

/**
 * @brief 采样率枚举
 * 定义了常用的音频采样率
 */
enum class SampleRate : int {
    RATE_8000 = 8000,      ///< 8kHz, 常用于语音通话
    RATE_16000 = 16000,    ///< 16kHz, 常用于语音识别
    RATE_24000 = 24000,    ///< 24kHz, 中等质量音频
    RATE_32000 = 32000,    ///< 32kHz, 中等质量音频
    RATE_44100 = 44100,    ///< 44.1kHz, CD音质
    RATE_48000 = 48000,    ///< 48kHz, 专业音频
    RATE_96000 = 96000,    ///< 96kHz, 高保真音频
    RATE_192000 = 192000   ///< 192kHz, 超高保真音频
};

/**
 * @brief 通道数枚举
 * 定义了音频通道配置
 */
enum class ChannelCount : int {
    MONO = 1,              ///< 单声道
    STEREO = 2             ///< 立体声
};

/**
 * @brief 采样格式枚举
 * 定义了音频采样格式
 */
enum class SampleFormat : int {
    INT16 = 0,             ///< 16位有符号整数
    INT24 = 1,             ///< 24位有符号整数
    INT32 = 2,             ///< 32位有符号整数
    FLOAT32 = 3,           ///< 32位浮点数
    FLOAT64 = 4            ///< 64位浮点数
};

/**
 * @brief 编码格式枚举
 * 定义了音频编码格式
 */
enum class EncodingFormat : int {
    WAV = 0,               ///< WAV格式，无损
    OPUS = 1,              ///< Opus格式，有损压缩
    RAW = 2                ///< 原始PCM数据
};

/**
 * @brief 设备类型枚举
 * 定义了音频设备类型
 */
enum class DeviceType : int {
    INPUT = 0,             ///< 输入设备
    OUTPUT = 1,            ///< 输出设备
    BOTH = 2               ///< 双向设备
};

/**
 * @brief 设备信息结构
 * 描述音频设备的硬件特性和能力
 */
struct DeviceInfo {
    int index;                          ///< 设备索引
    std::string name;                   ///< 设备名称
    DeviceType type;                    ///< 设备类型
    int maxInputChannels;               ///< 最大输入通道数
    int maxOutputChannels;              ///< 最大输出通道数
    double defaultSampleRate;           ///< 默认采样率
    double defaultLatency;              ///< 默认延迟(秒)
};

// VAD状态枚举
enum class VADState : int {
    IDLE = 0,           ///< 空闲状态
    SILENCE = 1,        ///< 静音状态
    SPEAKING = 2,       ///< 说话状态
    SENTENCE_END = 3    ///< 句子结束状态
};

// VAD配置结构
struct VADConfig {
    bool enabled = false;                  ///< 是否启用 VAD
    float threshold = 0.5f;               ///< VAD 阈值 (0.0-1.0)
    int silenceTimeoutMs = 500;           ///< 静音超时时间(毫秒)
    int sentenceTimeoutMs = 1000;         ///< 句子结束超时时间(毫秒)
    bool enableSilenceFrame = true;       ///< 是否启用静音帧
    bool enableSentenceDetection = true;  ///< 是否启用句子检测
    bool enableIdleDetection = true;      ///< 是否启用空闲检测

    // 序列化方法
    void fromJson(const std::string& jsonStr) {
        auto j = nlohmann::json::parse(jsonStr);
        enabled = j["enabled"];
        threshold = j["threshold"];
        silenceTimeoutMs = j["silenceTimeoutMs"];
        sentenceTimeoutMs = j["sentenceTimeoutMs"];
        enableSilenceFrame = j["enableSilenceFrame"];
        enableSentenceDetection = j["enableSentenceDetection"];
        enableIdleDetection = j["enableIdleDetection"];
    }

    std::string toJson() const {
        nlohmann::json j = {
            {"enabled", enabled},
            {"threshold", threshold},
            {"silenceTimeoutMs", silenceTimeoutMs},
            {"sentenceTimeoutMs", sentenceTimeoutMs},
            {"enableSilenceFrame", enableSilenceFrame},
            {"enableSentenceDetection", enableSentenceDetection},
            {"enableIdleDetection", enableIdleDetection}
        };
        return j.dump();
    }
};

// VAD状态结构
struct VADStatus {
    VADState state = VADState::IDLE;      ///< 当前 VAD 状态
    int64_t lastVoiceTime = 0;            ///< 最后一次检测到语音的时间
    int64_t currentSilenceDuration = 0;   ///< 当前静音持续时间
    bool isVoiceActive = false;           ///< 当前是否有语音活动
    int silenceFrameCount = 0;            ///< 连续静音帧计数
    int voiceFrameCount = 0;              ///< 连续语音帧计数
    float voiceProbability = 0.0f;        ///< 当前帧的语音概率
};

/**
 * @brief 音频配置结构
 * 定义音频流的处理参数和配置选项
 */
struct AudioConfig {
    // 基础参数
    SampleRate sampleRate = SampleRate::RATE_48000;     ///< 采样率
    ChannelCount channels = ChannelCount::MONO;         ///< 通道数
    SampleFormat format = SampleFormat::INT16;        ///< 采样格式
    int framesPerBuffer = 256;                          ///< 每缓冲帧数

    // 设备参数
    DeviceInfo inputDevice;                             ///< 输入设备信息
    DeviceInfo outputDevice;                            ///< 输出设备信息
    std::string recordingPath;                          ///< 录音文件保存路径

    // 编码参数
    EncodingFormat encodingFormat = EncodingFormat::WAV; ///< 编码格式
    int opusFrameLength = 20;                           ///< Opus帧长度(ms)
    int opusBitrate = 64000;                            ///< Opus比特率
    int opusComplexity = 10;                            ///< Opus复杂度

    // 处理参数
    bool enableVAD = false;                             ///< 是否启用语音检测
    bool enableAGC = false;                             ///< 是否启用自动增益控制
    VADConfig vadConfig;                                ///< VAD配置

    // 录音参数
    std::string outputFile;                             ///< 输出文件名
    bool autoStartRecording = false;                    ///< 是否自动开始录音
    int maxRecordingDuration = 0;                       ///< 最大录音时长(秒)

    // 序列化方法
    void fromJson(const std::string& jsonStr) {
        auto j = nlohmann::json::parse(jsonStr);
        sampleRate = static_cast<SampleRate>(j["sampleRate"]);
        channels = static_cast<ChannelCount>(j["channels"]);
        format = static_cast<SampleFormat>(j["format"]);
        framesPerBuffer = j["framesPerBuffer"];
        encodingFormat = static_cast<EncodingFormat>(j["encodingFormat"]);
        opusFrameLength = j["opusFrameLength"];
        opusBitrate = j["opusBitrate"];
        opusComplexity = j["opusComplexity"];
        enableVAD = j["enableVAD"];
        enableAGC = j["enableAGC"];
        vadConfig.fromJson(j["vadConfig"]);
        outputFile = j["outputFile"];
        autoStartRecording = j["autoStartRecording"];
        maxRecordingDuration = j["maxRecordingDuration"];
    }

    std::string toJson() const {
        nlohmann::json j = {
            {"sampleRate", static_cast<int>(sampleRate)},
            {"channels", static_cast<int>(channels)},
            {"format", static_cast<int>(format)},
            {"framesPerBuffer", framesPerBuffer},
            {"encodingFormat", static_cast<int>(encodingFormat)},
            {"opusFrameLength", opusFrameLength},
            {"opusBitrate", opusBitrate},
            {"opusComplexity", opusComplexity},
            {"enableVAD", enableVAD},
            {"enableAGC", enableAGC},
            {"vadConfig", vadConfig.toJson()},
            {"outputFile", outputFile},
            {"autoStartRecording", autoStartRecording},
            {"maxRecordingDuration", maxRecordingDuration}
        };
        return j.dump();
    }

    // 获取默认输入配置
    static AudioConfig getDefaultInputConfig() {
        AudioConfig config;
        config.sampleRate = SampleRate::RATE_48000;
        config.channels = ChannelCount::MONO;
        config.format = SampleFormat::INT16;
        config.framesPerBuffer = 256;
        config.encodingFormat = EncodingFormat::WAV;
        return config;
    }

    // 获取默认输出配置
    static AudioConfig getDefaultOutputConfig() {
        AudioConfig config;
        config.sampleRate = SampleRate::RATE_48000;
        config.channels = ChannelCount::STEREO;
        config.format = SampleFormat::FLOAT32;
        config.framesPerBuffer = 256;
        config.encodingFormat = EncodingFormat::WAV;
        return config;
    }
};

/**
 * @brief 音频回调函数类型
 */
using AudioCallback = std::function<void(const void* input, void* output, unsigned long frameCount)>;

/**
 * @brief WAV文件头结构体定义
 * 用于生成和解析WAV格式音频文件
 */
struct WavHeader {
    char riff[4];        // "RIFF" - 文件标识符
    uint32_t size;       // 文件大小 - 8
    char wave[4];        // "WAVE" - 格式标识符
    char fmt[4];         // "fmt " - 格式块标识符
    uint32_t fmtSize;    // fmt块大小
    uint16_t format;     // 1 = PCM格式
    uint16_t channels;   // 声道数
    uint32_t sampleRate; // 采样率
    uint32_t byteRate;   // 每秒字节数
    uint16_t blockAlign; // 块对齐
    uint16_t bitsPerSample; // 位深度
    char data[4];        // "data" - 数据块标识符
    uint32_t dataSize;   // 数据大小
};

/**
 * @brief OGG文件头结构体定义
 * 用于生成和解析OGG格式音频文件
 */
struct OggHeader {
    // OGG页面头
    struct {
        char capture_pattern[4];  // "OggS" - OGG标识符
        uint8_t stream_structure_version; // 流结构版本
        uint8_t header_type_flag; // 头部类型标志
        uint64_t granule_position; // 粒度位置
        uint32_t stream_serial_number; // 流序列号
        uint32_t page_sequence_number; // 页面序列号
        uint32_t page_checksum; // 页面校验和
        uint8_t page_segments; // 页面段数
    } page_header;

    // OpusHead包
    struct {
        char magic[8];           // "OpusHead" - Opus标识符
        uint8_t version;         // 版本号
        uint8_t channels;        // 声道数
        uint16_t pre_skip;       // 预跳过采样数
        uint32_t sample_rate;    // 采样率
        int16_t output_gain;     // 输出增益
        uint8_t channel_mapping; // 声道映射
    } opus_head;

    // OpusTags包
    struct {
        char magic[8];           // "OpusTags" - 标签标识符
        uint32_t vendor_length;  // 供应商字符串长度
        char vendor_string[32];  // 供应商字符串
        uint32_t user_comment_list_length; // 用户注释列表长度
    } opus_tags;
};

/**
 * @brief Opus帧长度选项结构
 * 定义了不同帧长度对应的采样数和描述信息
 */
struct OpusFrameOption {
    const char* description;    // 选项描述
    int lengthMs;              // 帧长度(毫秒)
    int samples48k;            // 48kHz采样率下的采样数
};

/**
 * @brief 预定义的Opus帧长度选项
 * 基于libopus支持的标准值，从2.5ms到60ms
 */
static const OpusFrameOption OPUS_FRAME_OPTIONS[] = {
    {"2.5ms (ultra-low latency, music)", 2, 120},   // 2.5ms @ 48kHz = 120 samples
    {"5ms (very low latency)", 5, 240},             // 5ms @ 48kHz = 240 samples  
    {"10ms (low latency, voice)", 10, 480},         // 10ms @ 48kHz = 480 samples
    {"20ms (default, best for voice)", 20, 960},    // 20ms @ 48kHz = 960 samples
    {"40ms (balanced)", 40, 1920},                  // 40ms @ 48kHz = 1920 samples
    {"60ms (higher compression)", 60, 2880}         // 60ms @ 48kHz = 2880 samples
};

/**
 * @brief Opus比特率选项结构
 * 定义了不同比特率对应的描述信息
 */
struct OpusBitrateOption {
    const char* description;    // 选项描述
    int bitrate;               // 比特率(bps)
    int opusConstant;          // 对应的OPUS常量值
};

/**
 * @brief 预定义的Opus比特率选项
 * 基于libopus推荐值，从6kbps到128kbps
 */
static const OpusBitrateOption OPUS_BITRATE_OPTIONS[] = {
    {"6kbps (very low quality, extreme compression)", 6000, 6000},
    {"8kbps (low quality, narrowband voice)", 8000, 8000},
    {"16kbps (medium quality, voice only)", 16000, 16000},
    {"24kbps (good quality, voice)", 24000, 24000},
    {"32kbps (high quality, voice)", 32000, 32000},
    {"48kbps (very high quality, wideband)", 48000, 48000},
    {"64kbps (excellent quality)", 64000, 64000},
    {"128kbps (maximum quality, music)", 128000, 128000}
};

/**
 * @brief Opus应用类型选项结构
 * 定义了不同应用场景的优化选项
 */
struct OpusApplicationOption {
    const char* description;    // 选项描述
    int opusApplication;        // 对应OPUS_APPLICATION_*常量
};

/**
 * @brief 预定义的Opus应用类型选项
 * 包括VOIP、音频流和低延迟三种模式
 */
static const OpusApplicationOption OPUS_APPLICATION_OPTIONS[] = {
    {"Voice over IP (VOIP)", 2048},      // OPUS_APPLICATION_VOIP = 2048
    {"Audio streaming", 2049},           // OPUS_APPLICATION_AUDIO = 2049  
    {"Restricted low-delay", 2051}       // OPUS_APPLICATION_RESTRICTED_LOWDELAY = 2051
};

} // namespace audio
} // namespace perfx 