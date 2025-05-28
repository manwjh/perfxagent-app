#include "audio/audio_device.h"
#include <portaudio.h>
#include <QDebug>

namespace perfx {

AudioDevice::AudioDevice()
    : stream_(nullptr)
    , sampleRate_(44100)
    , channels_(2)
    , isRecording_(false)
{
    Pa_Initialize();
    updateDeviceList();
}

AudioDevice::~AudioDevice() {
    if (stream_) {
        Pa_CloseStream(stream_);
    }
    Pa_Terminate();
}

std::vector<InputDeviceInfo> AudioDevice::getInputDevices() const {
    std::vector<InputDeviceInfo> devices;
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            InputDeviceInfo info;
            info.id = i;
            info.name = deviceInfo->name;
            info.maxChannels = deviceInfo->maxInputChannels;
            info.defaultSampleRate = deviceInfo->defaultSampleRate;
            devices.push_back(info);
        }
    }
    return devices;
}

std::vector<OutputDeviceInfo> AudioDevice::getOutputDevices() const {
    std::vector<OutputDeviceInfo> devices;
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxOutputChannels > 0) {
            OutputDeviceInfo info;
            info.id = i;
            info.name = deviceInfo->name;
            info.maxChannels = deviceInfo->maxOutputChannels;
            info.defaultSampleRate = deviceInfo->defaultSampleRate;
            devices.push_back(info);
        }
    }
    return devices;
}

bool AudioDevice::setInputDevice(int deviceId) {
    (void)deviceId;
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    return true;
}

bool AudioDevice::setOutputDevice(int deviceId) {
    (void)deviceId;
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    return true;
}

void AudioDevice::setSampleRate(int sampleRate) {
    sampleRate_ = sampleRate;
}

void AudioDevice::setChannels(int channels) {
    channels_ = channels;
}

void AudioDevice::setBitDepth(int bitDepth) {
    (void)bitDepth;
    // Not used in this implementation
}

void AudioDevice::setBufferSize(int bufferSize) {
    (void)bufferSize;
    // Not used in this implementation
}

bool AudioDevice::startRecording(AudioCallback callback) {
    if (isRecording_) {
        return false;
    }

    audioCallback_ = callback;
    isRecording_ = true;

    PaStreamParameters inputParameters;
    inputParameters.device = currentDevice_.id;
    inputParameters.channelCount = channels_;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_,
                              &inputParameters,
                              nullptr,
                              sampleRate_,
                              paFramesPerBufferUnspecified,
                              paClipOff,
                              paCallback,
                              this);

    if (err != paNoError) {
        qDebug() << "Failed to open stream:" << Pa_GetErrorText(err);
        isRecording_ = false;
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        qDebug() << "Failed to start stream:" << Pa_GetErrorText(err);
        isRecording_ = false;
        return false;
    }

    return true;
}

void AudioDevice::stopRecording() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    isRecording_ = false;
}

bool AudioDevice::playAudio(const float* data, int frames, std::function<void()> onFinished) {
    (void)data;
    (void)frames;
    if (isPlaying()) {
        return false;
    }

    playbackFinishedCallback_ = onFinished;

    PaStreamParameters outputParameters;
    outputParameters.device = currentDevice_.id;
    outputParameters.channelCount = channels_;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    PaError err = Pa_OpenStream(&stream_,
                              nullptr,
                              &outputParameters,
                              sampleRate_,
                              paFramesPerBufferUnspecified,
                              paClipOff,
                              paCallback,
                              this);

    if (err != paNoError) {
        qDebug() << "Failed to open stream:" << Pa_GetErrorText(err);
        return false;
    }

    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        qDebug() << "Failed to start stream:" << Pa_GetErrorText(err);
        return false;
    }

    return true;
}

void AudioDevice::stopPlayback() {
    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
}

bool AudioDevice::isRecording() const {
    return isRecording_;
}

bool AudioDevice::isPlaying() const {
    return stream_ != nullptr;
}

int AudioDevice::getCurrentInputDevice() const {
    return currentDevice_.id;
}

int AudioDevice::getCurrentOutputDevice() const {
    return currentDevice_.id;
}

AudioDeviceInfo AudioDevice::getCurrentDeviceInfo() const {
    return currentDevice_;
}

int AudioDevice::paCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
    (void)timeInfo;
    (void)statusFlags;
    AudioDevice* device = static_cast<AudioDevice*>(userData);
    
    if (inputBuffer && device->audioCallback_) {
        device->audioCallback_(static_cast<const float*>(inputBuffer), framesPerBuffer);
    }
    
    if (outputBuffer && device->playbackFinishedCallback_) {
        device->playbackFinishedCallback_();
    }
    
    return paContinue;
}

void AudioDevice::updateDeviceList() {
    inputDevices_.clear();
    int numDevices = Pa_GetDeviceCount();
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            AudioDeviceInfo info;
            info.id = i;
            info.name = deviceInfo->name;
            info.channels = deviceInfo->maxInputChannels;
            info.sampleRate = deviceInfo->defaultSampleRate;
            inputDevices_.push_back(info);
        }
    }
}

bool AudioDevice::initialize(int deviceId, int sampleRate, int channels) {
    (void)deviceId;
    (void)sampleRate;
    (void)channels;
    // TODO: 实现设备初始化逻辑
    return true;
}

bool AudioDevice::read(float* buffer, int frames) {
    (void)buffer;
    (void)frames;
    // TODO: 实现音频读取逻辑
    return true;
}

bool AudioDevice::write(const float* buffer, int frames) {
    (void)buffer;
    (void)frames;
    // TODO: 实现音频写入逻辑
    return true;
}

} // namespace perfx 