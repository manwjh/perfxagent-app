#include <portaudio.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

// 默认音频参数
#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_FRAMES_PER_BUFFER 256
#define DEFAULT_CHANNELS 1
#define DEFAULT_FORMAT paInt16

// WAV 文件头结构
struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t fileSize;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;  // PCM
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t dataSize;
};

// 写入 WAV 文件头
void writeWavHeader(std::ofstream& file, uint32_t sampleRate, uint16_t channels, uint32_t dataSize) {
    WavHeader header;
    header.fileSize = 36 + dataSize;
    header.numChannels = channels;
    header.sampleRate = sampleRate;
    header.byteRate = sampleRate * channels * 2;  // 16-bit = 2 bytes
    header.blockAlign = channels * 2;
    header.dataSize = dataSize;
    
    file.write(reinterpret_cast<char*>(&header), sizeof(WavHeader));
}

// 录音回调函数
int recordCallback(const void* inputBuffer, void* /*outputBuffer*/,
                  unsigned long framesPerBuffer,
                  const PaStreamCallbackTimeInfo* /*timeInfo*/,
                  PaStreamCallbackFlags /*statusFlags*/,
                  void* userData) {
    std::vector<int16_t>* buffer = static_cast<std::vector<int16_t>*>(userData);
    const int16_t* in = static_cast<const int16_t*>(inputBuffer);
    
    // 将数据添加到缓冲区
    buffer->insert(buffer->end(), in, in + framesPerBuffer * DEFAULT_CHANNELS);
    
    return paContinue;
}

int main() {
    // 初始化 PortAudio
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        std::cerr << "PortAudio error: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // 列出所有设备
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        std::cerr << "ERROR: Pa_CountDevices returned " << numDevices << std::endl;
        return 1;
    }

    std::cout << "\n=== Available Audio Devices ===\n";
    for (int i = 0; i < numDevices; ++i) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        std::cout << i << ": " << info->name << "\n";
        std::cout << "  Input channels: " << info->maxInputChannels << "\n";
        std::cout << "  Output channels: " << info->maxOutputChannels << "\n";
        std::cout << "  Default sample rate: " << info->defaultSampleRate << "\n";
        std::cout << "-------------------\n";
    }

    // 选择输入设备
    int deviceIndex;
    std::cout << "\nSelect input device index: ";
    std::cin >> deviceIndex;

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo || deviceInfo->maxInputChannels < 1) {
        std::cerr << "Invalid input device.\n";
        return 1;
    }

    // 配置输入参数
    PaStreamParameters inputParams;
    inputParams.device = deviceIndex;
    inputParams.channelCount = DEFAULT_CHANNELS;
    inputParams.sampleFormat = DEFAULT_FORMAT;
    inputParams.suggestedLatency = deviceInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    // 创建音频缓冲区
    std::vector<int16_t> audioBuffer;

    // 打开音频流
    PaStream* stream;
    err = Pa_OpenStream(
        &stream,
        &inputParams,
        nullptr,  // 无输出
        DEFAULT_SAMPLE_RATE,
        DEFAULT_FRAMES_PER_BUFFER,
        paClipOff,
        recordCallback,
        &audioBuffer
    );

    if (err != paNoError) {
        std::cerr << "Failed to open stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // 选择输出格式
    int formatChoice;
    std::cout << "\nSelect output format:\n";
    std::cout << "1. WAV\n";
    std::cout << "2. RAW\n";
    std::cout << "Choice (1-2): ";
    std::cin >> formatChoice;

    std::string outputFile = (formatChoice == 2) ? "recording.raw" : "recording.wav";
    std::cout << "\nRecording will be saved to: " << outputFile << std::endl;

    // 开始录音
    std::cout << "\nPress Enter to start recording...";
    std::cin.ignore();
    std::cin.get();

    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to start stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    std::cout << "Recording... Press Enter to stop." << std::endl;
    std::cin.get();

    // 停止录音
    err = Pa_StopStream(stream);
    if (err != paNoError) {
        std::cerr << "Failed to stop stream: " << Pa_GetErrorText(err) << std::endl;
        return 1;
    }

    // 保存录音
    if (formatChoice == 1) {  // WAV
        std::ofstream wavFile(outputFile, std::ios::binary);
        if (!wavFile.is_open()) {
            std::cerr << "Failed to open output file" << std::endl;
            return 1;
        }

        uint32_t dataSize = audioBuffer.size() * sizeof(int16_t);
        writeWavHeader(wavFile, DEFAULT_SAMPLE_RATE, DEFAULT_CHANNELS, dataSize);
        wavFile.write(reinterpret_cast<char*>(audioBuffer.data()), dataSize);
        wavFile.close();
    } else {  // RAW
        std::ofstream rawFile(outputFile, std::ios::binary);
        if (!rawFile.is_open()) {
            std::cerr << "Failed to open output file" << std::endl;
            return 1;
        }
        rawFile.write(reinterpret_cast<char*>(audioBuffer.data()), 
                     audioBuffer.size() * sizeof(int16_t));
        rawFile.close();
    }

    // 清理资源
    Pa_CloseStream(stream);
    Pa_Terminate();

    std::cout << "Recording saved to " << outputFile << std::endl;
    return 0;
} 