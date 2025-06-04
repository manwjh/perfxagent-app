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

/**
 * @brief 音频配置结构
 * 定义音频流的处理参数和配置选项
 */
struct AudioConfig {
    // 基础参数
    SampleRate sampleRate = SampleRate::RATE_48000;     ///< 采样率
    ChannelCount channels = ChannelCount::MONO;         ///< 通道数
    SampleFormat format = SampleFormat::INT16;          ///< 采样格式
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
    bool enableResampling = false;                      ///< 是否启用重采样
    SampleRate targetSampleRate = SampleRate::RATE_48000; ///< 目标采样率
    bool enableVAD = false;                             ///< 是否启用语音检测
    bool enableAGC = false;                             ///< 是否启用自动增益控制

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
        enableResampling = j["enableResampling"];
        targetSampleRate = static_cast<SampleRate>(j["targetSampleRate"]);
        enableVAD = j["enableVAD"];
        enableAGC = j["enableAGC"];
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
            {"enableResampling", enableResampling},
            {"targetSampleRate", static_cast<int>(targetSampleRate)},
            {"enableVAD", enableVAD},
            {"enableAGC", enableAGC},
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
        config.format = SampleFormat::INT16;
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

} // namespace audio
} // namespace perfx 