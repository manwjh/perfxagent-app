#pragma once

#include <portaudio.h>
#include <vector>
#include <string>
#include <functional>
#include <memory>

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

    // 获取所有可用的音频输入设备
    std::vector<AudioDeviceInfo> getInputDevices();
    
    // 设置当前音频设备
    bool setDevice(int deviceId);
    
    // 获取当前设备信息
    AudioDeviceInfo getCurrentDeviceInfo() const;
    
    // 开始录音
    void startRecording(std::function<void(const float*, int)> callback);
    
    // 停止录音
    void stopRecording();
    
    // 播放音频
    void playAudio(const float* data, int frames);

    void setSampleRate(int rate);
    int getSampleRate() const;

private:
    static int paCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData);

    PaStream* stream_;
    std::vector<AudioDeviceInfo> inputDevices_;
    AudioDeviceInfo currentDevice_;
    std::function<void(const float*, int)> audioCallback_;
    int sampleRate_;
    int channels_;
    bool isRecording_;
}; 