# Perfx Audio Module

这是 Perfx 项目的音频处理模块，提供了音频设备管理、音频处理和音频线程管理等功能。

## 功能特性

- 音频设备管理（输入/输出设备枚举和配置）
- 音频处理（VAD、重采样、编解码）
- 音频线程管理
- 支持多种音频格式和采样率
- 基于 PortAudio 的设备管理
- 使用 Opus 进行音频编解码
- 使用 WebRTC VAD 进行语音活动检测
- 使用 libsamplerate 进行音频重采样

## 依赖项

- PortAudio (>= 19.7.0)
- Opus
- WebRTC VAD
- libsamplerate
- nlohmann/json

## 安装依赖

### macOS
```bash
brew install portaudio opus libsamplerate
```

### Linux
```bash
sudo apt-get install libportaudio2 libopus0 libsamplerate0
```

## 使用示例

```cpp
#include "audio/audio_manager.h"

// 初始化音频管理器
auto& manager = perfx::audio::AudioManager::getInstance();
manager.initialize();

// 获取可用设备
auto devices = manager.getAvailableDevices();

// 创建音频配置
perfx::audio::AudioConfig config;
config.sampleRate = perfx::audio::SampleRate::RATE_48000;
config.channels = perfx::audio::ChannelCount::STEREO;
config.format = perfx::audio::SampleFormat::FLOAT32;
config.framesPerBuffer = 256;

// 创建音频线程
auto thread = manager.createAudioThread(config);

// 设置输入设备
thread->setInputDevice(devices[0]);

// 添加音频处理器
thread->addProcessor(manager.getProcessor());

// 开始录音
thread->start();
```

## 构建

音频模块作为 Perfx 项目的一部分进行构建。请参考主项目的构建说明。

## 许可证

与主项目相同的许可证。 