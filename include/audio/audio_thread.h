#pragma once

#include "audio/audio_device.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <functional>
#include <vector>

namespace perfx {

enum class AudioThreadState {
    IDLE,       // 空闲状态
    LISTENING,  // 聆听状态
    PLAYING,    // 播放状态
    PAUSED,     // 暂停状态
    STOPPED     // 停止状态
};

struct AudioThreadConfig {
    int sampleRate = 16000;
    int channels = 1;
    int deviceId = -1;  // -1 表示使用默认设备
    float vadThreshold = 0.5f;
    int bufferSize = 15 * 16000;  // 15秒的缓冲区大小
};

struct AudioStats {
    int bufferSize;
    bool isVadActive;
    AudioThreadState currentState;
    int framesProcessed;
};

class AudioThread {
public:
    AudioThread();
    ~AudioThread();

    // 初始化和配置
    bool initialize(const AudioThreadConfig& config);
    void setVadCallback(std::function<void(bool)> callback);

    // 状态控制
    bool startListening();
    bool startPlaying(const std::vector<float>& audioData);
    void pause();
    void stop();
    void resume();

    // 状态查询
    AudioThreadState getState() const;
    AudioStats getStats() const;
    std::vector<float> getAudioBuffer() const;

private:
    void threadFunction();
    void processAudioInput(const float* input, int frames);
    bool isVadActive(const float* input, int frames);

    AudioDevice audioDevice_;
    AudioThreadConfig config_;
    std::thread thread_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<AudioThreadState> state_;
    
    std::vector<float> audioBuffer_;
    std::queue<std::vector<float>> playbackQueue_;
    
    std::function<void(bool)> vadCallback_;
    bool shouldStop_;
    
    // 统计信息
    std::atomic<int> framesProcessed_;
    std::atomic<bool> vadActive_;
};

} // namespace perfx 