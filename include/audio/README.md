# PerfXAgent 音频模块使用说明

## 1. 模块概述

音频模块提供了完整的音频采集、处理和播放功能，支持以下特性：
- 音频设备管理（输入/输出设备枚举和选择）
- 音频流处理（VAD、重采样、Opus编解码等）
- 音频文件操作（WAV/Opus格式读写）
- 自动线程管理（采集、处理、播放）

## 2. 核心组件

### 2.1 AudioManager
音频管理器，负责整个音频模块的初始化和配置管理。

```cpp
// 获取单例实例
auto& manager = perfx::audio::AudioManager::getInstance();

// 初始化
manager.initialize();

// 获取可用设备列表
auto devices = manager.getAvailableDevices();

// 更新配置
AudioConfig config;
config.sampleRate = SampleRate::RATE_48000;
config.channels = ChannelCount::MONO;
config.format = SampleFormat::FLOAT32;
manager.updateConfig(config);
```

### 2.2 AudioDevice
音频设备管理，负责设备枚举和底层驱动操作。

```cpp
// 获取设备列表
auto devices = manager.getAvailableDevices();

// 设备信息包含：
// - 设备ID
// - 设备名称
// - 设备类型（输入/输出）
// - 最大输入/输出通道数
// - 默认采样率
```

### 2.3 AudioProcessor
音频处理器，提供音频流处理功能。

```cpp
// 获取处理器实例
auto processor = manager.getProcessor();

// 音频处理示例
std::vector<float> inputData;
std::vector<float> outputData;
processor->process(inputData, outputData);
```

### 2.4 AudioThread
音频线程，管理音频流的自动处理。

```cpp
// 创建音频线程
auto thread = manager.createAudioThread(config);

// 启动线程
thread->start();

// 停止线程
thread->stop();
```

## 3. 配置说明

### 3.1 音频配置（AudioConfig）
```cpp
struct AudioConfig {
    SampleRate sampleRate;     // 采样率
    ChannelCount channels;     // 通道数
    SampleFormat format;       // 采样格式
    EncodingFormat encoding;   // 编码格式
    int frameSize;            // 帧大小
    int bufferSize;           // 缓冲区大小
    bool enableNoiseReduction; // 降噪开关
    bool enableEchoCancellation; // 回声消除开关
    bool enableAutomaticGainControl; // 自动增益控制开关
};
```

### 3.2 支持的采样率
```cpp
enum class SampleRate {
    RATE_8000 = 8000,
    RATE_16000 = 16000,
    RATE_24000 = 24000,
    RATE_32000 = 32000,
    RATE_44100 = 44100,
    RATE_48000 = 48000,
    RATE_96000 = 96000
};
```

### 3.3 支持的采样格式
```cpp
enum class SampleFormat {
    FLOAT32,
    INT16,
    INT24,
    INT32,
    FLOAT64
};
```

## 4. 使用示例

### 4.1 基本录音示例
```cpp
// 初始化音频管理器
auto& manager = perfx::audio::AudioManager::getInstance();
manager.initialize();

// 配置录音参数
AudioConfig config;
config.sampleRate = SampleRate::RATE_48000;
config.channels = ChannelCount::MONO;
config.format = SampleFormat::FLOAT32;
config.encoding = EncodingFormat::WAV;

// 创建音频线程
auto thread = manager.createAudioThread(config);

// 开始录音
thread->start();

// ... 录音过程 ...

// 停止录音
thread->stop();
```

### 4.2 音频文件操作
```cpp
// 写入WAV文件
std::vector<float> audioData;
manager.writeWavFile(audioData.data(), audioData.size(), "output.wav");

// 写入Opus文件
manager.writeOpusFile(audioData.data(), audioData.size(), "output.opus");

// 读取WAV文件
std::vector<float> readData;
size_t frames;
manager.readWavFile("input.wav", readData, frames);
```

## 5. 注意事项

1. 设备兼容性
   - 不同设备支持的采样率和通道数可能不同
   - 建议在初始化时检查设备支持的具体参数

2. 性能考虑
   - 缓冲区大小会影响延迟和CPU使用率
   - 建议根据实际需求调整缓冲区大小

3. 错误处理
   - 所有操作都应该检查返回值
   - 初始化失败时应该及时清理资源

4. 线程安全
   - 音频线程是异步的
   - 注意线程间的数据同步

## 6. 依赖项

- PortAudio：音频设备驱动
- Opus：音频编码
- nlohmann-json：配置文件处理

## 7. 常见问题

1. 设备无法打开
   - 检查设备是否被其他程序占用
   - 确认设备权限设置

2. 音频质量问题
   - 检查采样率和格式设置
   - 确认设备支持的具体参数

3. 性能问题
   - 调整缓冲区大小
   - 检查CPU使用率
   - 考虑使用更高效的采样格式 