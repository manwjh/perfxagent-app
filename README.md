# PerfXAgent客户端应用程序

## 前言
- 这个项目起念是我想探索一下大模型的自动代码编程能力，纯属为了好玩而已。
- 我是一个"老"程序员，不过已经有十多年不写代码了。大模型唤醒了我写代码的灵魂，哈哈哈，因为不需要再花时间去复习代码的编程规范，同时也不需要去自己update的最新的知识。所以，我想尝试用一个全新的方式"写"项目，试图不写"一行"实际代码。更多故事，参考这里：https://github.com/manwjh/perfxagent-app/blob/main/PROJECT_HISTORY.md

## 项目简介
- PerfXAgent Cloud简介
该平台由澎峰长沙开发，目的是打造一个驱动各类硬件平台（电脑/手机/嵌入式）Agent云，你的硬件只要能支持Websocket通讯，用很少代码就能通过PerfXAgent获得丰富的、强大智能体灵魂。

PerfXAgent Cloud设计的理念是，将多模态MaaS接入，Agent这些功能全部放到云端。让App变成只是一个交互终端，App重点关注如何优化听觉，视觉，声音，界面这些和物理世界的交互方式，灵魂在云端。这可能会带来类似数据隐私、通讯延时等问题，不一定能覆盖所有场景，也没这个必要。

目前PerfXAgent已经支持的智能体：
1. chatbot（兼容小智AI）
2. 会议记录
3. 更多智能体正在研发...

- PerfXAgent APP和PerfXAgent Cloud的关系
1. PerfXAgent Cloud <--https/wss--> PerfXAgent App
2. 本项目（即perfxagent-app）目的是搭建一个运行在PC平台上的示范程序，最开始是在macos系统上开发的。该项目将最终支持MAC OS，Android，鸿蒙，Linux环境。
3. 是否可以连接更多类似PerfXAgent的服务器，可以讨论一下。

## 主要功能
1. 将本机作为一个智能设备绑定到用户在PerfXAgent Cloud的账号下。
2. 通过与PerfXAgent Cloud的通讯实现一些Agent，例如：
   - 音频到音频的ChatBot
   - 会议记录助手。

## 联系方式

- 项目主页：[GitHub](https://github.com/manwjh/perfxagent-app)
- 问题反馈：[Issues](https://github.com/manwjh/perfxagent-app/issues)

## 编译方法

### 依赖项
确保已安装以下依赖项：
- CMake (>= 3.14)
- C++17 兼容的编译器
- Qt6 (>= 6.4.0)
- PortAudio (>= 19.7.0)
- Opus
- SAMPLERATE
- nlohmann_json (>= 3.11.3)
- FFMPEG (libavcodec, libavformat, libavutil, libswresample)
- Boost (>= 1.74.0)
- OpenSSL (>= 3.0.0)

### 编译步骤
1. 克隆仓库并进入项目目录：
   ```bash
   git clone <repository-url>
   cd perfxagent-app
   ```

2. 创建并进入 build 目录：
   ```bash
   mkdir -p build
   cd build
   ```

3. 运行 CMake 配置：
   ```bash
   cmake ..
   ```

4. 编译项目：
   ```bash
   make -j4
   ```

5. 编译完成后，可执行文件位于 `build/bin/` 目录下。

## AUDIO 子模块和示例

### AUDIO 子模块
AUDIO 子模块是一个静态库，包含以下文件：
- 源文件：`src/audio/audio_device.cpp`, `src/audio/audio_processor.cpp`, `src/audio/audio_thread.cpp`, `src/audio/audio_manager.cpp`, `src/audio/audio_types.cpp`
- 头文件：`include/audio/audio_types.h`, `include/audio/audio_device.h`, `include/audio/audio_processor.h`, `include/audio/audio_thread.h`, `include/audio/audio_manager.h`

### 音频示例
项目包含两个音频示例程序：
- `audio_example`：使用 AUDIO 子模块的完整示例，位于 `examples/audio/audio_example.cpp`。
- `pa_record_example`：使用 PortAudio 的简单录音示例，位于 `examples/audio/pa_record_example.cpp`。

编译完成后，这两个示例程序位于 `build/bin/` 目录下，可以直接运行。