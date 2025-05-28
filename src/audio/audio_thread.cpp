#include "audio/audio_thread.h"
#include <algorithm>
#include <stdexcept>

namespace perfx {

AudioThread::AudioThread()
    : state_(AudioThreadState::IDLE)
    , shouldStop_(false)
    , framesProcessed_(0)
    , vadActive_(false) {
}

AudioThread::~AudioThread() {
    stop();
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool AudioThread::initialize(const AudioThreadConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != AudioThreadState::IDLE) {
        return false;
    }
    
    config_ = config;
    return audioDevice_.initialize(config.deviceId, config.sampleRate, config.channels);
}

void AudioThread::setVadCallback(std::function<void(bool)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    vadCallback_ = callback;
}

bool AudioThread::startListening() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != AudioThreadState::IDLE && state_ != AudioThreadState::STOPPED) {
        return false;
    }

    shouldStop_ = false;
    state_ = AudioThreadState::LISTENING;
    thread_ = std::thread(&AudioThread::threadFunction, this);
    return true;
}

bool AudioThread::startPlaying(const std::vector<float>& audioData) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != AudioThreadState::IDLE && state_ != AudioThreadState::STOPPED) {
        return false;
    }

    playbackQueue_.push(audioData);
    shouldStop_ = false;
    state_ = AudioThreadState::PLAYING;
    thread_ = std::thread(&AudioThread::threadFunction, this);
    return true;
}

void AudioThread::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == AudioThreadState::LISTENING || state_ == AudioThreadState::PLAYING) {
        state_ = AudioThreadState::PAUSED;
    }
}

void AudioThread::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    shouldStop_ = true;
    state_ = AudioThreadState::STOPPED;
    cv_.notify_all();
}

void AudioThread::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == AudioThreadState::PAUSED) {
        state_ = playbackQueue_.empty() ? AudioThreadState::LISTENING : AudioThreadState::PLAYING;
        cv_.notify_all();
    }
}

AudioThreadState AudioThread::getState() const {
    return state_;
}

AudioStats AudioThread::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    AudioStats stats;
    stats.bufferSize = audioBuffer_.size();
    stats.isVadActive = vadActive_;
    stats.currentState = state_;
    stats.framesProcessed = framesProcessed_;
    return stats;
}

std::vector<float> AudioThread::getAudioBuffer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return audioBuffer_;
}

void AudioThread::threadFunction() {
    while (!shouldStop_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] {
            return shouldStop_ || state_ != AudioThreadState::PAUSED;
        });

        if (shouldStop_) {
            break;
        }

        if (state_ == AudioThreadState::LISTENING) {
            std::vector<float> inputBuffer(config_.bufferSize);
            if (audioDevice_.read(inputBuffer.data(), inputBuffer.size())) {
                processAudioInput(inputBuffer.data(), inputBuffer.size());
            }
        } else if (state_ == AudioThreadState::PLAYING && !playbackQueue_.empty()) {
            std::vector<float> audioData = playbackQueue_.front();
            playbackQueue_.pop();
            audioDevice_.write(audioData.data(), audioData.size());
        }
    }
}

void AudioThread::processAudioInput(const float* input, int frames) {
    // 将输入数据添加到缓冲区
    audioBuffer_.insert(audioBuffer_.end(), input, input + frames);
    
    // 检查VAD
    bool vadActive = isVadActive(input, frames);
    if (vadActive != vadActive_) {
        vadActive_ = vadActive;
        if (vadCallback_) {
            vadCallback_(vadActive);
        }
    }
    
    framesProcessed_ += frames;
}

bool AudioThread::isVadActive(const float* input, int frames) {
    // 简单的能量检测VAD实现
    float energy = 0.0f;
    for (int i = 0; i < frames; ++i) {
        energy += input[i] * input[i];
    }
    energy /= frames;
    
    return energy > config_.vadThreshold;
}

} // namespace perfx 