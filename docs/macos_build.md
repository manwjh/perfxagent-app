# macOS 构建说明

本文档提供了在 macOS 环境下构建和运行 PerfXAgent 的详细说明。

## 系统要求

- macOS 10.15 (Catalina) 或更高版本
- Xcode 12.0 或更高版本
- CMake 3.16 或更高版本
- Qt 6.2.0 或更高版本
- PortAudio 19.7.0 或更高版本

## 环境准备

1. 安装 Xcode
   - 从 Mac App Store 安装 [Xcode](https://apps.apple.com/us/app/xcode/id497799835)
   - 安装完成后，打开 Xcode 并接受许可协议
   - 安装命令行工具：
     ```bash
     xcode-select --install
     ```

2. 安装 Homebrew
   - 打开终端，运行以下命令：
     ```bash
     /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
     ```
   - 将 Homebrew 添加到 PATH（如果安装脚本没有自动添加）：
     ```bash
     echo 'eval "$(/opt/homebrew/bin/brew shellenv)"' >> ~/.zprofile
     eval "$(/opt/homebrew/bin/brew shellenv)"
     ```

3. 安装 CMake
   ```bash
   brew install cmake
   ```

4. 安装 Qt
   ```bash
   brew install qt@6
   ```

5. 安装 PortAudio
   ```bash
   brew install portaudio
   ```

## 构建步骤

1. 克隆仓库
   ```bash
   git clone https://github.com/manwjh/perfxagent-app.git
   cd perfxagent-app
   ```

2. 创建构建目录
   ```bash
   mkdir build
   cd build
   ```

3. 配置项目
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt@6"
   ```

4. 构建项目
   ```bash
   make -j$(sysctl -n hw.ncpu)
   ```

## 运行程序

构建完成后，可执行文件将位于 `build/bin/perfxagent-app`。

运行程序：
```bash
./perfxagent-app
```

## 常见问题

1. 找不到 Qt 库
   - 确保正确设置了 CMAKE_PREFIX_PATH
   - 检查 Qt 是否通过 Homebrew 正确安装
   - 运行 `brew info qt@6` 查看安装信息

2. 找不到 PortAudio
   - 确保通过 Homebrew 安装了 PortAudio
   - 运行 `brew info portaudio` 查看安装信息

3. 权限问题
   - 如果遇到权限问题，确保已授予终端完全磁盘访问权限
   - 在系统偏好设置 > 安全性与隐私 > 隐私 > 完全磁盘访问权限中添加终端

4. 音频设备访问
   - 首次运行时，系统会请求麦克风访问权限
   - 在系统偏好设置 > 安全性与隐私 > 隐私 > 麦克风中允许应用访问

## 调试

1. 使用 Xcode 调试器
   - 在 Xcode 中打开项目：
     ```bash
     open -a Xcode perfxagent-app.xcodeproj
     ```
   - 设置断点并开始调试

2. 查看日志
   - 程序日志文件位于 `~/Library/Logs/perfxagent-app/`
   - 使用 Console.app 查看系统日志

## 注意事项

- 确保已授予应用必要的系统权限（麦克风、磁盘访问等）
- 如果使用 M1/M2 芯片的 Mac，确保所有依赖都支持 ARM 架构
- 建议使用 Release 配置进行最终构建
- 如果遇到音频设备问题，检查系统声音设置和权限设置

## 卸载

如果需要卸载应用及其依赖：

1. 删除应用
   ```bash
   rm -rf /Applications/perfxagent-app.app
   ```

2. 删除依赖（如果需要）
   ```bash
   brew uninstall qt@6 portaudio
   ```

3. 删除配置文件
   ```bash
   rm -rf ~/Library/Application\ Support/perfxagent-app
   rm -rf ~/Library/Logs/perfxagent-app
   ``` 