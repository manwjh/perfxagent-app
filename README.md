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

## 技术架构

### 前端技术栈
- Electron (用于构建跨平台桌面应用)
- React (用户界面开发)
- WebSocket (实时通信)
- Web Audio API (音频处理和播放)
- MediaRecorder API (音频采集)
- WebAssembly (音频处理算法)

### 通信协议
详细的WebSocket通信协议请参考 [websocket-protocol.md](./websocket-protocol.md)

## 系统要求

### Windows
- Windows 10 或更高版本
- Visual Studio 2019 或更高版本
- CMake 3.16 或更高版本
- Qt 6.2.0 或更高版本
- PortAudio 19.7.0 或更高版本

### macOS
- macOS 10.15 (Catalina) 或更高版本
- Xcode 12.0 或更高版本
- CMake 3.16 或更高版本
- Qt 6.2.0 或更高版本
- PortAudio 19.7.0 或更高版本

## 快速开始

1. 克隆仓库
   ```bash
   git clone https://github.com/manwjh/perfxagent-app.git
   cd perfxagent-app
   ```

2. 构建项目
   - Windows: 参考 [Windows 构建说明](docs/windows_build.md)
   - macOS: 参考 [macOS 构建说明](docs/macos_build.md)

3. 运行程序
   - Windows: `build/bin/Release/perfxagent-app.exe`
   - macOS: `build/bin/perfxagent-app`

## 使用说明

### 音频录制

1. 启动程序后，选择输入设备
2. 配置音频参数（采样率、通道数、位深度）
3. 按住录音按钮开始录音
4. 松开按钮结束录音并自动播放

### 设备配置

1. 在设置界面选择输入/输出设备
2. 配置设备参数
3. 使用测试按钮验证设备
4. 保存配置

### VAD 设置

1. 在设置界面调整 VAD 阈值
2. 实时查看 VAD 状态
3. 根据需要调整灵敏度

## 文档

- [音频设置说明](docs/audio_settings.md)
- [Windows 构建说明](docs/windows_build.md)
- [macOS 构建说明](docs/macos_build.md)

## 常见问题

### 音频设备问题

- 确保已授予应用麦克风访问权限
- 检查系统音频设置
- 验证设备驱动是否正确安装

### 构建问题

- 确保所有依赖都已正确安装
- 检查 CMake 配置
- 验证 Qt 和 PortAudio 安装

### 运行时问题

- 检查日志文件
- 验证系统权限
- 确认设备连接状态

## 开发

### 项目结构

```
perfxagent-app/
├── docs/           # 文档
├── include/        # 头文件
├── src/           # 源代码
│   ├── audio/     # 音频处理
│   ├── core/      # 核心功能
│   └── ui/        # 用户界面
├── tests/         # 测试
└── CMakeLists.txt # 构建配置
```

### 构建系统

- 使用 CMake 构建系统
- 支持跨平台构建
- 支持调试和发布配置

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 贡献

欢迎提交 Issue 和 Pull Request。

## 联系方式

- 项目主页：[GitHub](https://github.com/manwjh/perfxagent-app)
- 问题反馈：[Issues](https://github.com/manwjh/perfxagent-app/issues)