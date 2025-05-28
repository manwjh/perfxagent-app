#pragma once

#include <portaudio.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include "audio/audio_device_info.h"

namespace perfx {

struct AudioDeviceInfo {
    int id;
    std::string name;
    int channels;
    double sampleRate;
};

class AudioDevice {
public:
    AudioDevice();
    ~AudioDevice();

    // 设备管理
    std::vector<InputDeviceInfo> getInputDevices() const;
    std::vector<OutputDeviceInfo> getOutputDevices() const;
    bool setInputDevice(int deviceId);
    bool setOutputDevice(int deviceId);

    // 音频参数设置
    void setSampleRate(int sampleRate);
    void setChannels(int channels);
    void setBitDepth(int bitDepth);
    void setBufferSize(int bufferSize);

    // 录音和播放
    using AudioCallback = std::function<void(const float* data, int frames)>;
    bool startRecording(AudioCallback callback);
    void stopRecording();
    bool playAudio(const float* data, int frames, std::function<void()> onFinished = nullptr);
    void stopPlayback();

    // 设备状态
    bool isRecording() const;
    bool isPlaying() const;
    int getCurrentInputDevice() const;
    int getCurrentOutputDevice() const;

    // 获取当前设备信息
    AudioDeviceInfo getCurrentDeviceInfo() const;

    // 设备初始化
    bool initialize(int deviceId, int sampleRate, int channels);
    // 读写接口
    bool read(float* buffer, int frames);
    bool write(const float* buffer, int frames);

private:
    static int paCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData);

    void updateDeviceList();

    PaStream* stream_;
    std::vector<AudioDeviceInfo> inputDevices_;
    AudioDeviceInfo currentDevice_;
    std::function<void(const float*, int)> audioCallback_;
    std::function<void()> playbackFinishedCallback_;
    int sampleRate_;
    int channels_;
    bool isRecording_;
};

} // namespace perfx 