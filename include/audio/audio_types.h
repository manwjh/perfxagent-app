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

namespace perfx {
namespace audio {

/**
 * @brief 音频采样率枚举
 */
enum class SampleRate : int {
    RATE_8000 = 8000,
    RATE_16000 = 16000,
    RATE_24000 = 24000,
    RATE_32000 = 32000,
    RATE_44100 = 44100,
    RATE_48000 = 48000,
    RATE_96000 = 96000
};

/**
 * @brief 音频通道数枚举
 */
enum class ChannelCount : int {
    MONO = 1,
    STEREO = 2
};

/**
 * @brief 音频采样格式枚举
 */
enum class SampleFormat {
    FLOAT32,
    INT16,
    INT24,
    INT32,
    FLOAT64
};

/**
 * @brief 音频编码格式枚举
 */
enum class EncodingFormat {
    WAV,
    OPUS,
    RAW
};

/**
 * @brief 音频设备类型枚举
 */
enum class DeviceType {
    INPUT,
    OUTPUT,
    BOTH
};

/**
 * @brief 音频设备信息结构体
 */
struct DeviceInfo {
    int index;                  // 设备索引
    std::string name;          // 设备名称
    DeviceType type;           // 设备类型
    int maxInputChannels;      // 最大输入通道数
    int maxOutputChannels;     // 最大输出通道数
    double defaultSampleRate;  // 默认采样率
};

/**
 * @brief 音频配置结构体
 */
struct AudioConfig {
    SampleRate sampleRate;     // 采样率
    ChannelCount channels;     // 通道数
    SampleFormat format;       // 采样格式
    EncodingFormat encoding;   // 编码格式
    int frameSize;            // 帧大小
    int bufferSize;           // 缓冲区大小
    bool enableNoiseReduction; // 是否启用降噪
    bool enableEchoCancellation; // 是否启用回声消除
    bool enableAutomaticGainControl; // 是否启用自动增益控制

    // 兼容原有功能的成员
    int framesPerBuffer;                // 每缓冲帧数
    EncodingFormat encodingFormat;      // 编码格式（兼容）
    int opusFrameLength;                // Opus 帧长度
    DeviceInfo inputDevice;             // 输入设备
    DeviceInfo outputDevice;            // 输出设备
    std::string recordingPath;          // 录音保存路径
    bool autoStartRecording;            // 是否自动开始录音
    int maxRecordingDuration;           // 最大录音时长（秒）

    // 静态方法声明
    static AudioConfig getDefaultInputConfig();
    static AudioConfig getDefaultOutputConfig();

    // 成员方法声明
    void fromJson(const std::string& jsonStr);
    std::string toJson() const;
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