#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <portaudio.h>

namespace perfx {
namespace audio {

// 音频采样率
enum class SampleRate : int {
    RATE_8000 = 8000,
    RATE_16000 = 16000,
    RATE_24000 = 24000,
    RATE_32000 = 32000,
    RATE_44100 = 44100,
    RATE_48000 = 48000
};

// 音频通道数
enum class ChannelCount : int {
    MONO = 1,
    STEREO = 2
};

// 音频采样格式
enum class SampleFormat {
    FLOAT32,
    INT16,
    INT24,
    INT32
};

// 音频设备类型
enum class DeviceType {
    INPUT,
    OUTPUT,
    BOTH
};

// 音频设备信息
struct DeviceInfo {
    int index;
    std::string name;
    DeviceType type;
    int maxInputChannels;
    int maxOutputChannels;
    double defaultSampleRate;
};

// 音频配置
struct AudioConfig {
    // 基本配置
    SampleRate sampleRate = SampleRate::RATE_48000;
    ChannelCount channels = ChannelCount::STEREO;
    SampleFormat format = SampleFormat::FLOAT32;
    int framesPerBuffer = 256;

    // 设备配置
    DeviceInfo inputDevice;
    DeviceInfo outputDevice;

    // 录音配置
    std::string recordingPath;
    bool autoStartRecording = false;
    int maxRecordingDuration = 3600; // 1小时

    // 序列化方法
    std::string toJson() const;
    void fromJson(const std::string& json);

    // 静态方法
    static AudioConfig getDefaultInputConfig();
    static AudioConfig getDefaultOutputConfig();
};

// 音频缓冲区
class AudioBuffer {
public:
    AudioBuffer(size_t size);
    ~AudioBuffer();

    void* data() { return buffer_; }
    const void* data() const { return buffer_; }
    size_t size() const { return size_; }
    size_t bytesPerFrame() const { return bytesPerFrame_; }

private:
    void* buffer_;
    size_t size_;
    size_t bytesPerFrame_;
};

// 音频回调函数类型
using AudioCallback = std::function<void(const void* input, void* output, unsigned long frameCount)>;

} // namespace audio
} // namespace perfx 