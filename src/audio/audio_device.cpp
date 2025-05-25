#include "audio/audio_device.h"
#include <iostream>

namespace perfx {

AudioDevice::AudioDevice() 
    : stream_(nullptr)
    , sampleRate_(16000)
    , channels_(1)
    , isRecording_(false) {
    Pa_Initialize();
    inputDevices_ = getInputDevices();
}

AudioDevice::~AudioDevice() {
    if (stream_) {
        Pa_CloseStream(stream_);
    }
    Pa_Terminate();
}

std::vector<AudioDeviceInfo> AudioDevice::getInputDevices() {
    std::vector<AudioDeviceInfo> devices;
    int numDevices = Pa_GetDeviceCount();
    
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            AudioDeviceInfo info;
            info.id = i;
            info.name = deviceInfo->name;
            info.channels = deviceInfo->maxInputChannels;
            info.sampleRate = deviceInfo->defaultSampleRate;
            devices.push_back(info);
        }
    }
    return devices;
}

bool AudioDevice::setDevice(int deviceId) {
    try {
        auto it = std::find_if(inputDevices_.begin(), inputDevices_.end(),
            [deviceId](const AudioDeviceInfo& info) { return info.id == deviceId; });
        
        if (it != inputDevices_.end()) {
            currentDevice_ = *it;
            return true;
        }
        return false;
    } catch (const std::exception& e) {
        std::cerr << "设置音频设备失败: " << e.what() << std::endl;
        return false;
    }
}

AudioDeviceInfo AudioDevice::getCurrentDeviceInfo() const {
    return currentDevice_;
}

void AudioDevice::startRecording(std::function<void(const float*, int)> callback) {
    if (!currentDevice_.name.empty()) {
        audioCallback_ = callback;
        isRecording_ = true;

        PaStreamParameters inputParameters;
        inputParameters.device = currentDevice_.id;
        inputParameters.channelCount = channels_;
        inputParameters.sampleFormat = paFloat32;
        inputParameters.suggestedLatency = Pa_GetDeviceInfo(currentDevice_.id)->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;

        Pa_OpenStream(&stream_,
                     &inputParameters,
                     nullptr,
                     sampleRate_,
                     paFramesPerBufferUnspecified,
                     paClipOff,
                     paCallback,
                     this);

        Pa_StartStream(stream_);
    }
}

void AudioDevice::stopRecording() {
    if (stream_) {
        isRecording_ = false;
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

void AudioDevice::playAudio(const float* data, int frames) {
    if (!data || frames <= 0) return;

    PaStream* playStream = nullptr;
    PaStreamParameters outputParameters;
    
    outputParameters.device = Pa_GetDefaultOutputDevice();
    outputParameters.channelCount = channels_;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&playStream,
                 nullptr,
                 &outputParameters,
                 sampleRate_,
                 paFramesPerBufferUnspecified,
                 paClipOff | paDitherOff,
                 nullptr,
                 nullptr);
    
    if (err != paNoError) {
        std::cerr << "打开音频输出流失败: " << Pa_GetErrorText(err) << std::endl;
        return;
    }

    err = Pa_StartStream(playStream);
    if (err != paNoError) {
        std::cerr << "启动音频输出流失败: " << Pa_GetErrorText(err) << std::endl;
        Pa_CloseStream(playStream);
        return;
    }

    const int chunkSize = 1024;
    for (int i = 0; i < frames; i += chunkSize) {
        int remainingFrames = std::min(chunkSize, frames - i);
        err = Pa_WriteStream(playStream, data + i * channels_, remainingFrames);
        if (err != paNoError) {
            std::cerr << "写入音频数据失败: " << Pa_GetErrorText(err) << std::endl;
            break;
        }
        Pa_Sleep(10);
    }

    err = Pa_StopStream(playStream);
    if (err != paNoError) {
        std::cerr << "停止音频输出流失败: " << Pa_GetErrorText(err) << std::endl;
    }

    err = Pa_CloseStream(playStream);
    if (err != paNoError) {
        std::cerr << "关闭音频输出流失败: " << Pa_GetErrorText(err) << std::endl;
    }
}

int AudioDevice::paCallback(const void* inputBuffer, void* outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData) {
    AudioDevice* device = static_cast<AudioDevice*>(userData);
    if (device->isRecording_ && device->audioCallback_) {
        const float* input = static_cast<const float*>(inputBuffer);
        device->audioCallback_(input, framesPerBuffer);
    }
    return paContinue;
}

void AudioDevice::setSampleRate(int rate) {
    sampleRate_ = rate;
}

int AudioDevice::getSampleRate() const {
    return sampleRate_;
}

} // namespace perfx 