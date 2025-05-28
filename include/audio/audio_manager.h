#pragma once

#include "audio/audio_device_info.h"
#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace perfx {

// 音频配置结构
struct AudioConfig {
    int sampleRate = 16000;
    int channels = 1;
    AudioFormat format = AudioFormat::PCM_16BIT;
    int bufferSize = 1024;
    float vadThreshold = 0.5f;
    bool autoStartRecording = false;
    int maxRecordingDuration = 0;  // 0表示无限制
};

// 音频状态
enum class AudioState {
    IDLE,
    RECORDING,
    PLAYING,
    PAUSED
};

// 音频事件回调
struct AudioCallbacks {
    std::function<void(const float* data, int frames)> onAudioData;
    std::function<void(bool isActive)> onVadStateChanged;
    std::function<void(AudioState state)> onStateChanged;
};

class AudioManager {
public:
    static AudioManager& getInstance();

    // 初始化和清理
    bool initialize();
    void cleanup();

    // 设备管理
    std::vector<InputDeviceInfo> getInputDevices() const;
    std::vector<OutputDeviceInfo> getOutputDevices() const;
    bool setInputDevice(int deviceId);
    bool setOutputDevice(int deviceId);
    int getCurrentInputDevice() const;
    int getCurrentOutputDevice() const;

    // 配置管理
    void setConfig(const AudioConfig& config);
    AudioConfig getConfig() const;
    bool saveConfig(const std::string& filePath) const;
    bool loadConfig(const std::string& filePath);

    // 录音控制
    bool startRecording();
    void stopRecording();
    bool isRecording() const;
    void setRecordingCallbacks(const AudioCallbacks& callbacks);

    // 播放控制
    bool playAudio(const std::vector<float>& data);
    void stopPlayback();
    bool isPlaying() const;

    // 状态查询
    AudioState getState() const;
    std::string getLastError() const;

private:
    AudioManager();
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    class Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace perfx 