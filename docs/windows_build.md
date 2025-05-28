# Windows 构建说明

本文档提供了在 Windows 环境下构建和运行 PerfXAgent 的详细说明。

## 系统要求

- Windows 10 或更高版本
- Visual Studio 2019 或更高版本（推荐使用 Visual Studio 2022）
- CMake 3.16 或更高版本
- Qt 6.2.0 或更高版本
- PortAudio 19.7.0 或更高版本

## 环境准备

1. 安装 Visual Studio
   - 下载并安装 [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/)
   - 在安装时选择"使用 C++ 的桌面开发"工作负载

2. 安装 CMake
   - 下载并安装 [CMake](https://cmake.org/download/)
   - 确保将 CMake 添加到系统 PATH

3. 安装 Qt
   - 下载并安装 [Qt Online Installer](https://www.qt.io/download-qt-installer)
   - 选择 Qt 6.2.0 或更高版本
   - 确保安装 MSVC 2019 64-bit 组件

4. 安装 PortAudio
   - 使用 vcpkg 安装：
     ```powershell
     vcpkg install portaudio:x64-windows
     ```

## 构建步骤

1. 克隆仓库
   ```powershell
   git clone https://github.com/manwjh/perfxagent-app.git
   cd perfxagent-app
   ```

2. 创建构建目录
   ```powershell
   mkdir build
   cd build
   ```

3. 配置项目
   ```powershell
   cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.2.0/msvc2019_64"
   ```
   注意：请根据你的 Qt 安装路径调整 CMAKE_PREFIX_PATH

4. 构建项目
   ```powershell
   cmake --build . --config Release
   ```

## 运行程序

构建完成后，可执行文件将位于 `build/bin/Release/perfxagent-app.exe`。

## 音频设备配置

1. 输入设备
   - 支持选择系统默认输入设备
   - 支持选择外部音频设备
   - 支持配置采样率（16000Hz/48000Hz）
   - 支持配置通道数（单声道/立体声）
   - 支持配置位深度（16bit/24bit/32bit）

2. 输出设备
   - 支持选择系统默认输出设备
   - 支持选择外部音频设备
   - 支持配置采样率和通道数

3. VAD 配置
   - 支持语音活动检测
   - 可配置 VAD 阈值
   - 实时显示 VAD 状态

## 常见问题

1. 找不到 Qt 库
   - 确保正确设置了 CMAKE_PREFIX_PATH
   - 检查 Qt 版本是否与 Visual Studio 版本匹配

2. 找不到 PortAudio
   - 确保正确安装了 PortAudio
   - 检查环境变量是否正确设置

3. 运行时缺少 DLL
   - 确保 Qt 的 bin 目录在系统 PATH 中
   - 检查是否所有必要的 DLL 都已复制到程序目录

4. 音频设备访问
   - 确保已授予应用麦克风访问权限
   - 检查 Windows 隐私设置中的麦克风权限
   - 如果使用外部音频设备，确保设备已正确连接并被系统识别

## 调试

1. 使用 Visual Studio 调试器
   - 在 Visual Studio 中打开解决方案文件
   - 设置断点并开始调试

2. 查看日志
   - 程序日志文件位于 `%APPDATA%/perfxagent-app/logs/`

## 注意事项

- 确保使用管理员权限运行程序（如果需要访问音频设备）
- 如果遇到音频设备问题，请检查 Windows 音频设置
- 建议使用 Release 配置进行最终构建
- 支持保存和加载音频配置预设
- 支持实时音频设备切换和参数调整
- 如果使用外部音频设备，确保设备驱动已正确安装 