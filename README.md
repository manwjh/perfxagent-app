# PerfxAgent-ASR

一个基于Qt6的跨平台桌面应用程序，专注于音频处理、实时语音识别(ASR)和智能转录功能。

A Qt6-based cross-platform desktop application focused on audio processing, real-time speech recognition (ASR), and intelligent transcription features.

## 🚀 功能特性 / Features

- 🎤 **音频处理**: 高质量音频录制、播放和格式转换
- 🗣️ **实时语音识别**: 集成火山引擎ASR服务，支持实时语音转文字
- 📝 **智能转录**: 支持歌词同步格式，可导出LRC和JSON格式
- 🎵 **音频可视化**: 实时波形显示和音频分析
- 🔌 **多协议支持**: 基于IXWebSocket的稳定网络通信
- 🖥️ **现代化UI**: 基于Qt6的图形用户界面，支持多窗口管理
- ⚡ **高性能**: 异步处理，支持流式音频处理
- 🎨 **资源管理**: 完整的图标和UI资源管理系统
- 🔐 **安全配置**: 多种API密钥配置方式，支持混淆保护

- 🎤 **Audio Processing**: High-quality audio recording, playback, and format conversion
- 🗣️ **Real-time Speech Recognition**: Integrated Volcengine ASR service with real-time speech-to-text
- 📝 **Intelligent Transcription**: Supports LRC synchronization format, exportable in LRC and JSON formats
- 🎵 **Audio Visualization**: Real-time waveform display and audio analysis
- 🔌 **Multi-protocol Support**: Stable network communication based on IXWebSocket
- 🖥️ **Modern UI**: Qt6-based graphical user interface with multi-window management
- ⚡ **High Performance**: Asynchronous processing with streaming audio support
- 🎨 **Resource Management**: Complete icon and UI resource management system
- 🔐 **Secure Configuration**: Multiple API key configuration methods with obfuscation protection

## 🛠️ 系统要求 / System Requirements

- **操作系统**: macOS 10.15+
- **编译器**: 支持C++17的编译器 (Apple Clang 12+)
- **CMake**: 3.10或更高版本
- **Qt**: Qt6 (Core, Widgets, Network, Gui, Multimedia, WebEngineWidgets, WebSockets, Concurrent, PrintSupport)

- **Operating System**: macOS 10.15+
- **Compiler**: C++17 compatible compiler (Apple Clang 12+)
- **CMake**: Version 3.10 or higher
- **Qt**: Qt6 (Core, Widgets, Network, Gui, Multimedia, WebEngineWidgets, WebSockets, Concurrent, PrintSupport)

### macOS依赖安装 / macOS Dependencies Installation

```bash
# 使用Homebrew安装依赖 / Install dependencies using Homebrew
brew install qt@6
brew install openssl@3
brew install portaudio
brew install boost
brew install opus
brew install libogg
brew install nlohmann-json
brew install websocketpp
brew install asio
brew install opencv
brew install libsndfile
```

## 🏗️ 构建步骤 / Build Steps

### 🚀 快速开始 / Quick Start

#### 开发模式构建（推荐） / Development Build (Recommended)
```bash
# 一键开发模式构建，自动启用调试功能
./scripts/build_dev.sh
```

#### 生产模式构建 / Production Build
```bash
# 一键生产模式构建和打包
./scripts/build_package.sh
```

#### 调试模式构建 / Debug Build
```bash
# 构建调试版本并启用所有调试功能
./scripts/build_package.sh --debug
```

### 手动构建步骤 / Manual Build Steps

#### 1. 开发模式 / Development Mode
```bash
# 清理并创建构建目录
rm -rf build && mkdir build && cd build

# 配置项目（Debug模式）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 编译项目
make -j$(sysctl -n hw.ncpu)

# 运行应用程序（自动启用调试模式）
cd ..
export ASR_DEBUG=1
export ASR_ENABLE_BUSINESS_LOG=1
export ASR_ENABLE_FLOW_LOG=1
export ASR_ENABLE_DATA_LOG=1
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

#### 2. 生产模式 / Production Mode
```bash
# 清理并创建构建目录
rm -rf build && mkdir build && cd build

# 配置项目（Release模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译项目
make -j$(sysctl -n hw.ncpu)

# 创建安装包
cpack -G "DragNDrop;TGZ"

# 运行应用程序
cd ..
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

### 构建验证 / Build Verification

编译成功后，你应该看到以下输出：
After successful compilation, you should see the following outputs:

- 主可执行文件: `PerfxAgent-ASR.app`
- 音频处理库: `libperfx_audio.a`
- ASR客户端库: `libperfx_asr_client.a`
- ASR管理库: `libperfx_asr_manager.a`

- Main executable: `PerfxAgent-ASR.app`
- Audio processing library: `libperfx_audio.a`
- ASR client library: `libperfx_asr_client.a`
- ASR manager library: `libperfx_asr_manager.a`

## 🎯 核心模块 / Core Modules

### ASR模块 (自动语音识别) / ASR Module (Automatic Speech Recognition)

**核心组件 / Core Components**:
- **AsrManager**: 高级管理类，提供简化的API接口
- **AsrClient**: 底层WebSocket客户端，处理与ASR服务的通信
- **AsrCallback**: 回调接口，处理识别结果和状态变化

- **AsrManager**: High-level management class providing simplified API interface
- **AsrClient**: Low-level WebSocket client handling communication with ASR service
- **AsrCallback**: Callback interface for processing recognition results and status changes

**支持功能 / Supported Features**:
- 文件音频识别
- 实时流式识别
- 多种客户端实现 (IXWebSocket, Qt, WebSocketpp)
- 配置管理和凭据管理

- File audio recognition
- Real-time streaming recognition
- Multiple client implementations (IXWebSocket, Qt, WebSocketpp)
- Configuration and credential management

**使用示例 / Usage Example**:
```cpp
#include <asr/asr_manager.h>

// 创建管理器和回调 / Create manager and callback
Asr::AsrManager manager;
SimpleCallback callback;
manager.setCallback(&callback);

// 一行代码完成音频识别 / One-line audio recognition
bool success = manager.recognizeAudioFile("audio.wav");
```

### 音频处理模块 / Audio Processing Module

**核心组件 / Core Components**:
- **AudioManager**: 音频管理单例，统一管理音频操作
- **AudioDevice**: 音频设备管理
- **AudioProcessor**: 音频数据处理
- **AudioThread**: 音频处理线程

- **AudioManager**: Audio management singleton for unified audio operations
- **AudioDevice**: Audio device management
- **AudioProcessor**: Audio data processing
- **AudioThread**: Audio processing thread

**支持功能 / Supported Features**:
- 多格式音频录制 (WAV, Opus)
- 实时音频流处理
- 音频设备管理
- 波形数据生成
- 歌词同步格式支持

- Multi-format audio recording (WAV, Opus)
- Real-time audio stream processing
- Audio device management
- Waveform data generation
- LRC synchronization format support

### 实时转录控制器 / Real-time Transcription Controller

**核心组件 / Core Components**:
- **RealtimeTranscriptionController**: 实时转录控制逻辑
- **RealtimeAsrCallback**: 实时ASR回调处理

- **RealtimeTranscriptionController**: Real-time transcription control logic
- **RealtimeAsrCallback**: Real-time ASR callback processing

**支持功能 / Supported Features**:
- 实时音频录制
- 实时ASR识别
- 转录文本累积
- 波形可视化
- 多设备支持

- Real-time audio recording
- Real-time ASR recognition
- Transcription text accumulation
- Waveform visualization
- Multi-device support

## 📁 项目结构 / Project Structure

```
perfxagent-app/
├── include/                    # 头文件 / Header files
│   ├── asr/                   # 语音识别相关 / Speech recognition related
│   │   ├── asr_client.h       # ASR客户端 / ASR client
│   │   ├── asr_manager.h      # ASR管理器 / ASR manager
│   │   ├── asr_debug_config.h # ASR调试配置 / ASR debug config
│   │   └── asr_log_utils.h    # ASR日志工具 / ASR log utilities
│   ├── audio/                 # 音频处理相关 / Audio processing related
│   │   ├── audio_manager.h    # 音频管理器 / Audio manager
│   │   ├── audio_device.h     # 音频设备 / Audio device
│   │   ├── audio_processor.h  # 音频处理器 / Audio processor
│   │   ├── audio_thread.h     # 音频线程 / Audio thread
│   │   ├── audio_types.h      # 音频类型定义 / Audio type definitions
│   │   └── file_importer.h    # 文件导入器 / File importer
│   ├── logic/                 # 业务逻辑 / Business logic
│   │   └── realtime_transcription_controller.h # 实时转录控制器 / Real-time transcription controller
│   └── ui/                    # 用户界面相关 / User interface related
│       ├── main_window.h      # 主窗口 / Main window
│       ├── audio_to_text_window.h      # 音频转文字窗口 / Audio to text window
│       ├── realtime_audio_to_text_window.h # 实时音频转文字窗口 / Real-time audio to text window
│       └── app_icon_button.h  # 应用图标按钮 / App icon button
├── src/                       # 源代码 / Source code
│   ├── asr/                   # ASR实现 / ASR implementation
│   ├── audio/                 # 音频处理实现 / Audio processing implementation
│   ├── logic/                 # 业务逻辑实现 / Business logic implementation
│   ├── ui/                    # UI实现 / UI implementation
│   ├── main.cpp               # 主程序入口 / Main program entry
│   └── main.js                # Electron主进程 / Electron main process
├── resources/                 # 资源文件 / Resource files
│   ├── icons/                 # 应用图标 / Application icons
│   │   ├── app_icon.ico       # Windows图标 / Windows icon
│   │   ├── app_icon.icns      # macOS图标 / macOS icon
│   │   └── ui_icons/          # UI图标 / UI icons
│   ├── images/                # 图片资源 / Image resources
│   │   ├── backgrounds/       # 背景图片 / Background images
│   │   └── logos/             # Logo变体 / Logo variations
│   ├── fonts/                 # 字体文件 / Font files
│   └── README.md              # 资源说明 / Resource documentation
├── scripts/                   # 工具脚本 / Utility scripts
│   ├── icon_generator.sh      # 图标生成脚本 / Icon generation script
│   ├── build_dev.sh           # 开发模式构建脚本 / Development build script
│   ├── build_package.sh       # 打包脚本 / Packaging script
│   └── README.md              # 脚本说明 / Script documentation
├── third_party/              # 第三方库 / Third-party libraries
│   └── ixwebsocket/         # IXWebSocket库 / IXWebSocket library
├── sample/                   # 示例音频文件 / Sample audio files
├── cmake/                    # CMake配置 / CMake configuration
├── bin/                      # 编译输出目录 / Build output directory
├── package.json              # Electron配置 / Electron configuration
└── PROJECT_HISTORY*.md       # 项目开发历史 / Project development history
```

## 🎵 支持格式 / Supported Formats

### 音频格式 / Audio Formats
- **输入格式**: WAV, MP3, FLAC, OGG
- **输出格式**: WAV, Opus
- **采样率**: 8kHz - 48kHz
- **声道**: 单声道/立体声
- **编码**: PCM, Opus, MP3

- **Input Formats**: WAV, MP3, FLAC, OGG
- **Output Formats**: WAV, Opus
- **Sample Rate**: 8kHz - 48kHz
- **Channels**: Mono/Stereo
- **Encoding**: PCM, Opus, MP3

### 转录格式 / Transcription Formats
- **LRC格式**: 标准歌词同步格式
- **JSON格式**: 结构化数据格式
- **实时文本**: 支持中间结果和最终结果

- **LRC Format**: Standard lyrics synchronization format
- **JSON Format**: Structured data format
- **Real-time Text**: Supports intermediate and final results

## 🔧 配置说明 / Configuration

### API密钥配置 / API Key Configuration

#### 配置方式优先级 / Configuration Priority

1. **环境变量**（最高优先级 / Highest Priority）
   ```bash
   export ASR_APP_ID="your_app_id"
   export ASR_ACCESS_TOKEN="your_access_token"
   export ASR_SECRET_KEY="your_secret_key"
   ```

2. **配置文件**（推荐 / Recommended）
   ```bash
   # 复制模板文件
   cp config/api_keys_template.json config/api_keys.json
   
   # 编辑配置文件，填入真实密钥
   # Edit the config file with your real API keys
   ```

3. **UI界面配置**（通过应用程序设置 / Via Application Settings）
   - 在应用程序的系统配置界面中设置
   - 配置保存在 `~/.perfxagent/asr_config.json`

4. **混淆代码**（体验模式 / Trial Mode）
   - 使用内置的混淆API密钥
   - 仅用于测试和体验

#### 生成混淆数据 / Generate Obfuscated Data

```bash
# 运行混淆工具（支持多种输入方式）
python3 scripts/generate_obfuscated_keys.py

# 工具会自动检测：
# - config/api_keys.json 配置文件
# - ASR_* 环境变量
# - 交互式输入
```

#### 安全注意事项 / Security Notes

- ✅ `config/api_keys_template.json` - 可以提交到Git（模板文件）
- ❌ `config/api_keys.json` - 不要提交到Git（包含真实密钥）
- ✅ 使用环境变量作为替代方案
- ✅ 使用UI界面配置作为替代方案

### ASR配置 / ASR Configuration

ASR模块支持多种配置选项：
The ASR module supports various configuration options:

```cpp
Asr::AsrConfig config;
config.url = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel";
config.appId = "your_app_id";
config.accessToken = "your_access_token";
config.secretKey = "your_secret_key";
config.sampleRate = 16000;
config.language = "zh-CN";
```

### 音频配置 / Audio Configuration

```cpp
audio::AudioConfig config;
config.sampleRate = 16000;
config.channels = ChannelCount::MONO;
config.bitsPerSample = 16;
config.format = EncodingFormat::WAV;
```

## 🚀 快速开始 / Quick Start

### 0. 图标资源准备 / Icon Resource Preparation

在开始开发之前，建议先生成应用图标：
Before starting development, it's recommended to generate application icons first:

```bash
# 1. 将您的LOGO文件放到指定位置
# Place your LOGO file at the specified location
cp your_logo.png resources/icons/PerfxAgent-ASR.png

# 2. 运行图标生成脚本
# Run the icon generation script
./scripts/icon_generator.sh

# 3. 检查生成的图标文件
# Check the generated icon files
ls -la resources/icons/
```

生成的图标包括：
Generated icons include:
- PNG格式：16x16 到 1024x1024 各种尺寸
- ICO格式：Windows应用图标
- ICNS格式：macOS应用图标

- PNG format: Various sizes from 16x16 to 1024x1024
- ICO format: Windows application icon
- ICNS format: macOS application icon

### 1. 音频文件识别 / Audio File Recognition

使用主应用程序的"音频转文字"功能：
Use the main application's "Audio to Text" feature:

- 导入音频文件
- 自动识别并转换为文字
- 支持多种输出格式

- Import audio files
- Automatic recognition and text conversion
- Support for multiple output formats

### 2. 实时语音识别 / Real-time Speech Recognition

```bash
# 运行主应用程序 / Run the main application
cd build
./perfxagent-app
```

在主界面中选择"实时音频转文字"功能，开始实时语音识别。
Select "Real-time Audio to Text" in the main interface to start real-time speech recognition.

### 3. 音频录制和转换 / Audio Recording and Conversion

使用主应用程序的"音频转文字"功能，可以：
Using the main application's "Audio to Text" feature, you can:

- 录制音频
- 导入音频文件
- 转换为文字
- 导出为多种格式

- Record audio
- Import audio files
- Convert to text
- Export in multiple formats

## 🔍 调试和故障排除 / Debugging and Troubleshooting

### 启用调试模式 / Enable Debug Mode

#### 方法1：使用开发模式构建脚本（推荐）
```bash
# 自动启用所有调试功能
./scripts/build_dev.sh
```

#### 方法2：手动设置环境变量
```bash
# 设置环境变量启用ASR调试
export ASR_DEBUG=1
export ASR_ENABLE_BUSINESS_LOG=1
export ASR_ENABLE_FLOW_LOG=1
export ASR_ENABLE_DATA_LOG=1
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

#### 方法3：构建时启用调试
```bash
# Debug构建自动启用调试功能
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(sysctl -n hw.ncpu)
cd ..
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

### 调试功能说明 / Debug Features

- **ASR_DEBUG=1**: 启用ASR模块的详细调试日志
- **ASR_ENABLE_BUSINESS_LOG=1**: 启用业务逻辑日志
- **ASR_ENABLE_FLOW_LOG=1**: 启用流程控制日志
- **ASR_ENABLE_DATA_LOG=1**: 启用数据处理日志
- **ASR_ENABLE_PROTOCOL_LOG=1**: 启用协议通信日志
- **ASR_ENABLE_AUDIO_LOG=1**: 启用音频处理日志

## 📦 macOS打包 / macOS Packaging

### 创建macOS应用包 / Create macOS Application Package

```bash
# 使用一键打包脚本（推荐）
./scripts/build_package.sh

# 或者手动创建安装包
cd build
cpack -G "DragNDrop;TGZ"
cd ..
```

### 使用Electron打包 / Using Electron Packaging

```bash
# 安装依赖 / Install dependencies
npm install

# 开发模式 / Development mode
npm run dev

# 构建应用 / Build application
npm run build
```

## 📊 版本信息 / Version Information

- **当前版本**: 1.6.2
- **Qt版本**: Qt6
- **C++标准**: C++17
- **支持平台**: macOS (主要), Linux, Windows (计划中)

- **Current Version**: 1.6.2
- **Qt Version**: Qt6
- **C++ Standard**: C++17
- **Supported Platforms**: macOS (Primary), Linux, Windows (Planned)

## 📄 许可证 / License

本项目采用MIT许可证。详情请查看项目根目录下的许可证文件。
This project is licensed under the MIT License. See the license file in the project root directory for details.

## 🤝 贡献 / Contributing

欢迎提交Issue和Pull Request来改进这个项目。
Welcome to submit Issues and Pull Requests to improve this project.

### 贡献指南 / Contributing Guidelines

1. Fork项目 / Fork the project
2. 创建功能分支 / Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. 提交更改 / Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 / Push to the branch (`git push origin feature/AmazingFeature`)
5. 打开Pull Request / Open a Pull Request

## 📞 支持 / Support

如果遇到问题，请联系作者：manwjh@126.com
If you encounter any issues, please contact the author: manwjh@126.com

1. 查看项目开发日记文档 / Check project development diary documents (`PROJECT_HISTORY*.md`)
2. 检查项目的Issue页面 / Check the project's Issues page
3. 提交新的Issue描述问题 / Submit a new Issue describing the problem

## 🔗 相关链接 / Related Links

- [火山引擎ASR API文档](https://www.volcengine.com/docs/82379) / [Volcengine ASR API Documentation](https://www.volcengine.com/docs/82379)
- [Qt6官方文档](https://doc.qt.io/qt-6/) / [Qt6 Official Documentation](https://doc.qt.io/qt-6/)
- [IXWebSocket库文档](https://github.com/machinezone/IXWebSocket) / [IXWebSocket Library Documentation](https://github.com/machinezone/IXWebSocket)
- [Electron官方文档](https://www.electronjs.org/docs) / [Electron Official Documentation](https://www.electronjs.org/docs)