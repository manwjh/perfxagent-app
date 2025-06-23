# PerfxAgent App

一个基于Qt6的跨平台桌面应用程序，专注于音频处理、实时语音识别(ASR)和智能转录功能。

## 🚀 功能特性

- 🎤 **音频处理**: 高质量音频录制、播放和格式转换
- 🗣️ **实时语音识别**: 集成火山引擎ASR服务，支持实时语音转文字
- 📝 **智能转录**: 支持歌词同步格式，可导出LRC和JSON格式
- 🎵 **音频可视化**: 实时波形显示和音频分析
- 🔌 **多协议支持**: 基于IXWebSocket的稳定网络通信
- 🖥️ **现代化UI**: 基于Qt6的图形用户界面，支持多窗口管理
- ⚡ **高性能**: 异步处理，支持流式音频处理

## 🛠️ 系统要求

- **操作系统**: macOS 10.15+, Linux (Ubuntu 18.04+), Windows 10+
- **编译器**: 支持C++17的编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake**: 3.10或更高版本
- **Qt**: Qt6 (Core, Widgets, Network, Gui, Multimedia, WebEngineWidgets, WebSockets, Concurrent, PrintSupport)

### macOS依赖安装

```bash
# 使用Homebrew安装依赖
brew install qt@6
brew install openssl@3
brew install portaudio
brew install boost
brew install opus
brew install libogg
brew install nlohmann-json
brew install websocketpp
brew install asio
```

### Linux依赖安装

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential cmake
sudo apt-get install qt6-base-dev qt6-multimedia-dev qt6-webengine-dev
sudo apt-get install libportaudio2 libportaudio-dev
sudo apt-get install libopus-dev libogg-dev
sudo apt-get install nlohmann-json3-dev
sudo apt-get install libboost-all-dev
```

## 🏗️ 构建步骤

### 主应用程序

1. **克隆项目**
   ```bash
   git clone <repository-url>
   cd perfxagent-app
   ```

2. **创建构建目录**
   ```bash
   mkdir build
   cd build
   ```

3. **配置项目**
   ```bash
   cmake ..
   ```

4. **编译项目**
   ```bash
   make -j$(nproc)
   ```

5. **运行应用程序**
   ```bash
   ./perfxagent-app
   ```

### 示例项目

项目包含多个示例程序，可以单独编译和运行：

#### ASR简单示例

演示如何用一行代码完成音频文件到文本的转换：

```bash
# 编译ASR示例
cd examples
mkdir build
cd build
cmake ..
make asr_simple_example

# 运行ASR示例
./asr_simple_example [音频文件路径]
```

**ASR示例功能**:
- 🎯 **一行代码调用**: `manager.recognizeAudioFile(audioFile)` 完成完整识别流程
- 🔄 **异步处理**: 支持实时回调处理识别结果
- 📝 **结果提取**: 自动解析JSON格式的识别结果
- ⚡ **WebSocket通信**: 基于IXWebSocket的稳定连接

#### 其他示例

```bash
# 编译所有示例
cd examples/build
make

# 运行特定示例
./audio_example          # 音频处理示例
./asr_ixwebsocket_example # ASR WebSocket示例
./realtime_asr_test      # 实时ASR测试
```

## 🎯 核心模块

### ASR模块 (自动语音识别)

**核心组件**:
- **AsrManager**: 高级管理类，提供简化的API接口
- **AsrClient**: 底层WebSocket客户端，处理与ASR服务的通信
- **AsrCallback**: 回调接口，处理识别结果和状态变化

**支持功能**:
- 文件音频识别
- 实时流式识别
- 多种客户端实现 (IXWebSocket, Qt, WebSocketpp)
- 配置管理和凭据管理

**使用示例**:
```cpp
#include <asr/asr_manager.h>

// 创建管理器和回调
Asr::AsrManager manager;
SimpleCallback callback;
manager.setCallback(&callback);

// 一行代码完成音频识别
bool success = manager.recognizeAudioFile("audio.wav");
```

### 音频处理模块

**核心组件**:
- **AudioManager**: 音频管理单例，统一管理音频操作
- **AudioDevice**: 音频设备管理
- **AudioProcessor**: 音频数据处理
- **AudioThread**: 音频处理线程

**支持功能**:
- 多格式音频录制 (WAV, Opus)
- 实时音频流处理
- 音频设备管理
- 波形数据生成
- 歌词同步格式支持

### 实时转录控制器

**核心组件**:
- **RealtimeTranscriptionController**: 实时转录控制逻辑
- **RealtimeAsrCallback**: 实时ASR回调处理

**支持功能**:
- 实时音频录制
- 实时ASR识别
- 转录文本累积
- 波形可视化
- 多设备支持

## 📁 项目结构

```
perfxagent-app/
├── include/                    # 头文件
│   ├── asr/                   # 语音识别相关
│   │   ├── asr_client.h       # ASR客户端
│   │   ├── asr_manager.h      # ASR管理器
│   │   ├── asr_debug_config.h # ASR调试配置
│   │   └── asr_log_utils.h    # ASR日志工具
│   ├── audio/                 # 音频处理相关
│   │   ├── audio_manager.h    # 音频管理器
│   │   ├── audio_device.h     # 音频设备
│   │   ├── audio_processor.h  # 音频处理器
│   │   ├── audio_thread.h     # 音频线程
│   │   ├── audio_types.h      # 音频类型定义
│   │   └── file_importer.h    # 文件导入器
│   ├── logic/                 # 业务逻辑
│   │   └── realtime_transcription_controller.h # 实时转录控制器
│   └── ui/                    # 用户界面相关
│       ├── main_window.h      # 主窗口
│       ├── audio_to_text_window.h      # 音频转文字窗口
│       ├── realtime_audio_to_text_window.h # 实时音频转文字窗口
│       └── app_icon_button.h  # 应用图标按钮
├── src/                       # 源代码
│   ├── asr/                   # ASR实现
│   ├── audio/                 # 音频处理实现
│   ├── logic/                 # 业务逻辑实现
│   ├── ui/                    # UI实现
│   └── main.cpp               # 主程序入口
├── examples/                  # 示例代码
│   ├── asr_simple_example.cpp      # ASR简单示例
│   ├── asr_ixwebsocket_example.cpp # ASR WebSocket示例
│   ├── realtime_asr_test.cpp       # 实时ASR测试
│   ├── audio/                      # 音频处理示例
│   └── README.md                   # 示例使用指南
├── third_party/              # 第三方库
│   └── ixwebsocket/         # IXWebSocket库
├── sample/                   # 示例音频文件
├── cmake/                    # CMake配置
└── bin/                      # 编译输出目录
```

## 🎵 支持格式

### 音频格式
- **输入格式**: WAV, MP3, FLAC, OGG
- **输出格式**: WAV, Opus
- **采样率**: 8kHz - 48kHz
- **声道**: 单声道/立体声
- **编码**: PCM, Opus, MP3

### 转录格式
- **LRC格式**: 标准歌词同步格式
- **JSON格式**: 结构化数据格式
- **实时文本**: 支持中间结果和最终结果

## 🔧 配置说明

### ASR配置

ASR模块支持多种配置选项：

```cpp
Asr::AsrConfig config;
config.url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
config.appId = "your_app_id";
config.accessToken = "your_access_token";
config.secretKey = "your_secret_key";
config.sampleRate = 16000;
config.language = "zh-CN";
```

### 音频配置

```cpp
audio::AudioConfig config;
config.sampleRate = 16000;
config.channels = ChannelCount::MONO;
config.bitsPerSample = 16;
config.format = EncodingFormat::WAV;
```

## 🚀 快速开始

### 1. 音频文件识别

```bash
# 编译并运行ASR示例
cd examples/build
./asr_simple_example sample/38s.wav
```

### 2. 实时语音识别

```bash
# 运行主应用程序
cd build
./perfxagent-app
```

在主界面中选择"实时音频转文字"功能，开始实时语音识别。

### 3. 音频录制和转换

使用主应用程序的"音频转文字"功能，可以：
- 录制音频
- 导入音频文件
- 转换为文字
- 导出为多种格式

## 🔍 调试和故障排除

### 启用调试模式

```bash
# 设置环境变量启用ASR调试
export ASR_DEBUG=1
./perfxagent-app
```

### 常见问题

1. **Qt6未找到**
   ```bash
   # macOS
   brew install qt@6
   
   # Linux
   sudo apt-get install qt6-base-dev
   ```

2. **音频设备问题**
   - 检查系统音频权限
   - 确认音频设备正常工作
   - 查看音频设备列表

3. **ASR连接失败**
   - 检查网络连接
   - 验证ASR凭据
   - 确认防火墙设置

## 📊 版本信息

- **当前版本**: 1.6.1
- **Qt版本**: Qt6
- **C++标准**: C++17
- **支持平台**: macOS, Linux, Windows

## 📄 许可证

本项目采用MIT许可证。详情请查看项目根目录下的许可证文件。

## 🤝 贡献

欢迎提交Issue和Pull Request来改进这个项目。

### 贡献指南

1. Fork项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 打开Pull Request

## 📞 支持

如果遇到问题，请：

1. 查看 `examples/README.md` 中的详细使用指南
2. 检查项目的Issue页面
3. 提交新的Issue描述问题

## 🔗 相关链接

- [火山引擎ASR API文档](https://www.volcengine.com/docs/82379)
- [Qt6官方文档](https://doc.qt.io/qt-6/)
- [IXWebSocket库文档](https://github.com/machinezone/IXWebSocket)