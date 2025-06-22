# PerfxAgent App

一个基于Qt6的跨平台应用程序，提供音频处理、摄像头控制和自动语音识别(ASR)功能。

## 功能特性

- 🎤 **音频处理**: 音频录制、播放和格式转换
- 📹 **摄像头控制**: 摄像头设备管理和视频捕获
- 🗣️ **语音识别**: 集成ASR服务，支持实时语音转文字
- 🖥️ **现代化UI**: 基于Qt6的图形用户界面
- 🔌 **WebSocket支持**: 使用IXWebSocket库进行网络通信

## 系统要求

- **操作系统**: macOS, Linux, Windows
- **编译器**: 支持C++17的编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- **CMake**: 3.10或更高版本
- **Qt**: Qt6 (Core, Widgets, Network, Gui, Multimedia, WebEngineWidgets, WebSockets)

### macOS依赖

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

## 构建步骤

### 主应用程序

1. **克隆项目**
   ```bash
   git clone <repository-url>
   cd perfxagent-app-1.0.1_已上传为1.2.0
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

### 示例项目

项目包含多个示例程序，可以单独编译和运行：

#### ASR简单示例

ASR（自动语音识别）模块提供了一个简单的示例，演示如何用一行代码完成音频文件到文本的转换：

```bash
# 编译ASR示例
cd examples
mkdir build
cd build
cmake ..
make asr_simple_example

# 运行ASR示例
./asr_simple_example
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
./camera_example         # 摄像头控制示例
./asr_ixwebsocket_example # ASR WebSocket示例
```

## ASR模块详解

### 核心组件

- **AsrManager**: 高级管理类，提供简化的API接口
- **AsrClient**: 底层WebSocket客户端，处理与ASR服务的通信
- **AsrCallback**: 回调接口，处理识别结果和状态变化

### 使用示例

```cpp
#include <asr/asr_manager.h>

// 创建管理器和回调
Asr::AsrManager manager;
SimpleCallback callback;
manager.setCallback(&callback);

// 一行代码完成音频识别
bool success = manager.recognizeAudioFile("audio.wav");
```

### 支持格式

- **音频格式**: WAV, MP3, FLAC, OGG
- **采样率**: 8kHz - 48kHz
- **声道**: 单声道/立体声
- **编码**: PCM, Opus, MP3

## 运行

### 主应用程序

编译完成后，在build目录中运行：

```bash
./perfxagent-app
```

### 示例程序

```bash
# ASR示例
cd examples/build
./asr_simple_example

# 音频示例
./audio_example

# 摄像头示例
./camera_example
```

## 项目结构

```
├── include/          # 头文件
│   ├── asr/         # 语音识别相关
│   │   ├── asr_client.h    # ASR客户端
│   │   └── asr_manager.h   # ASR管理器
│   ├── audio/       # 音频处理相关
│   ├── camera/      # 摄像头相关
│   └── ui/          # 用户界面相关
├── src/             # 源代码
├── examples/        # 示例代码
│   ├── asr_simple_example.cpp      # ASR简单示例
│   ├── asr_ixwebsocket_example.cpp # ASR WebSocket示例
│   ├── audio_example.cpp           # 音频处理示例
│   └── camera_example.cpp          # 摄像头示例
├── third_party/     # 第三方库
└── docs/           # 文档
```

## 版本信息

当前版本: 1.5.0

## 许可证

请查看项目根目录下的许可证文件。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。

## 支持

如果遇到问题，请查看 `docs/` 目录下的文档或提交Issue。 