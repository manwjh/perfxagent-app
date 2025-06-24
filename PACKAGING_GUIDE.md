# PerfxAgent-ASR macOS 安装包打包指南

## 📋 概述

本指南详细说明了如何为 PerfxAgent-ASR 应用程序创建 macOS 安装包，包括 DMG 镜像文件和压缩包。PerfxAgent-ASR 是一个基于 Qt6 的跨平台桌面应用程序，专注于音频处理、实时语音识别(ASR)和智能转录功能。

## 🎯 打包目标

- **DMG 镜像文件**: 用户友好的拖拽安装体验
- **TGZ 压缩包**: 命令行安装和分发
- **自动依赖处理**: 包含所有必要的 Qt6 和第三方库
- **代码签名**: 支持 Apple Developer 证书签名
- **现代化UI**: 基于 Qt6 的图形用户界面，支持多窗口管理

## 🛠️ 系统要求

### 开发环境
- macOS 10.15+ (Catalina)
- Xcode Command Line Tools
- Homebrew
- CMake 3.10+
- 支持 C++17 的编译器 (Apple Clang 12+)

### 核心依赖库
```bash
# Qt6 核心组件
brew install qt@6

# 音频处理相关
brew install portaudio
brew install opus
brew install libogg
brew install libsndfile

# 网络和加密
brew install openssl@3
brew install websocketpp
brew install asio

# 数据处理
brew install boost
brew install nlohmann-json

# 计算机视觉 (可选)
brew install opencv
```

## 🚀 快速开始

### 1. 一键构建
```bash
# 运行完整的构建和打包流程
./scripts/build_package.sh
```

### 2. 手动构建步骤
```bash
# 清理并创建构建目录
rm -rf build && mkdir build && cd build

# 配置项目
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译项目
make -j$(sysctl -n hw.ncpu)

# 创建安装包
cpack -G "DragNDrop;TGZ"
```

## 📁 项目结构

```
perfxagent-app/
├── cmake/                          # CMake 配置文件
│   ├── MacOSXBundleInfo.plist.in   # Bundle 信息模板
│   ├── setup_dmg_ds_store.sh       # DMG 背景设置脚本
│   ├── create_app_icon.sh          # 图标生成脚本
│   ├── deploy_dependencies.sh      # 依赖部署脚本
│   ├── app_icon.icns              # 应用程序图标
│   └── dmg_background.png         # DMG 背景图片
├── scripts/                        # 构建脚本
│   ├── build_package.sh           # 主构建脚本
│   ├── icon_generator.sh          # 图标生成脚本
│   └── README.md                  # 脚本说明
├── resources/                      # 资源文件
│   ├── icons/                     # 应用图标
│   │   ├── app_icon.icns          # macOS 图标
│   │   ├── app_icon.ico           # Windows 图标
│   │   ├── app_icon_1024x1024.png # 高分辨率图标
│   │   └── PerfxAgent-ASR.png     # 源图标文件
│   ├── images/                    # 图片资源
│   ├── fonts/                     # 字体文件
│   └── README.md                  # 资源说明
├── include/                        # 头文件
│   ├── asr/                       # 语音识别相关
│   ├── audio/                     # 音频处理相关
│   ├── logic/                     # 业务逻辑
│   └── ui/                        # 用户界面相关
├── src/                           # 源代码
│   ├── asr/                       # ASR 实现
│   ├── audio/                     # 音频处理实现
│   ├── logic/                     # 业务逻辑实现
│   ├── ui/                        # UI 实现
│   ├── main.cpp                   # 主程序入口
│   └── CMakeLists.txt             # 源码构建配置
├── third_party/                   # 第三方库
│   └── ixwebsocket/              # IXWebSocket 库
├── sample/                        # 示例音频文件
├── package.json                   # Electron 配置 (可选)
├── LICENSE                        # 许可证文件
└── CMakeLists.txt                 # 主 CMake 配置
```

## ⚙️ 配置选项

### CMake 配置变量

| 变量名 | 默认值 | 描述 |
|--------|--------|------|
| `CMAKE_BUILD_TYPE` | Debug | 构建类型 (Debug/Release) |
| `PERFXAGENT_VERSION` | 1.6.2 | 应用程序版本号 |
| `CPACK_PACKAGE_NAME` | PerfxAgent-ASR | 包名称 |
| `CPACK_GENERATOR` | DragNDrop;TGZ | 包生成器类型 |
| `MACOSX_BUNDLE_GUI_IDENTIFIER` | com.perfx.agent.asr | Bundle 标识符 |

### 环境变量

```bash
# 设置 Qt6 路径
export Qt6_DIR="/opt/homebrew/lib/cmake/Qt6"

# 设置 OpenSSL 路径
export OPENSSL_ROOT_DIR="/opt/homebrew/opt/openssl@3"

# 设置 Boost 路径
export BOOST_ROOT="/opt/homebrew"

# 启用 ASR 调试模式
export ASR_DEBUG=1
```

## 🎨 自定义配置

### 应用程序图标

1. 准备 1024x1024 PNG 图标文件
2. 放置到 `resources/icons/PerfxAgent-ASR.png`
3. 运行图标生成脚本：
```bash
./scripts/icon_generator.sh
```

生成的图标包括：
- PNG 格式：16x16 到 1024x1024 各种尺寸
- ICO 格式：Windows 应用图标
- ICNS 格式：macOS 应用图标

### DMG 背景图片

1. 准备 800x600 PNG 背景图片
2. 放置到 `resources/dmg_background.png`
3. 构建时会自动复制到 `cmake/dmg_background.png`

### 应用程序信息

编辑 `cmake/MacOSXBundleInfo.plist.in` 文件：
```xml
<key>CFBundleName</key>
<string>PerfxAgent-ASR</string>
<key>CFBundleIdentifier</key>
<string>com.perfx.agent.asr</string>
<key>CFBundleVersion</key>
<string>1.6.2</string>
<key>CFBundleShortVersionString</key>
<string>1.6.2</string>
```

## 🔧 高级配置

### 代码签名

1. 获取 Apple Developer 证书
2. 配置签名脚本：
```bash
# 在 deploy_dependencies.sh 中添加
codesign --force --deep --sign "Developer ID Application: Your Name" "${APP_BUNDLE}"
```

### 公证 (Notarization)

1. 上传应用程序到 Apple 进行公证
2. 配置公证脚本：
```bash
# 上传进行公证
xcrun altool --notarize-app --primary-bundle-id "com.perfx.agent.asr" \
  --username "your-apple-id" --password "app-specific-password" \
  --file "PerfxAgent-ASR.dmg"
```

### 自动化构建

创建 GitHub Actions 工作流：
```yaml
name: Build macOS Package
on: [push, pull_request]
jobs:
  build:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v3
      - name: Install dependencies
        run: |
          brew install qt@6 openssl@3 portaudio boost opus libogg nlohmann-json websocketpp asio opencv libsndfile
      - name: Build package
        run: ./scripts/build_package.sh
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: PerfxAgent-macOS
          path: build/PerfxAgent-*.dmg
```

## 🐛 故障排除

### 常见问题

1. **Qt6 找不到**
   ```bash
   # 确保 Qt6 已安装
   brew install qt@6
   # 设置 Qt6 路径
   export Qt6_DIR="/opt/homebrew/lib/cmake/Qt6"
   ```

2. **依赖库链接失败**
   ```bash
   # 检查库路径
   otool -L build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   # 修复库路径
   install_name_tool -change "old_path" "new_path" build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   ```

3. **CPack 生成失败**
   ```bash
   # 检查 CMake 配置
   cmake --build build --target package
   # 查看详细错误信息
   cpack --debug --verbose
   ```

4. **音频设备问题**
   - 检查系统音频权限
   - 确认音频设备正常工作
   - 查看音频设备列表

5. **ASR 连接失败**
   - 检查网络连接
   - 验证 ASR 凭据
   - 确认防火墙设置

### 调试技巧

1. **检查依赖关系**
   ```bash
   otool -L build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   ```

2. **验证应用程序包**
   ```bash
   codesign -dv --verbose=4 build/bin/PerfxAgent-ASR.app
   ```

3. **测试安装包**
   ```bash
   # 挂载 DMG
   hdiutil attach PerfxAgent-ASR-1.6.2.dmg
   # 测试应用程序
   /Volumes/PerfxAgent-ASR\ 1.6.2/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   ```

4. **启用调试模式**
   ```bash
   # 设置环境变量启用 ASR 调试
   export ASR_DEBUG=1
   ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   ```

## 📦 发布流程

### 1. 版本准备
```bash
# 更新版本号
sed -i '' 's/1.6.2/1.6.3/g' CMakeLists.txt
sed -i '' 's/1.6.1/1.6.3/g' package.json
```

### 2. 构建测试
```bash
# 完整构建测试
./scripts/build_package.sh
```

### 3. 代码签名和公证
```bash
# 签名应用程序
codesign --force --deep --sign "Developer ID Application: Your Name" build/bin/PerfxAgent-ASR.app

# 创建 DMG
hdiutil create -volname "PerfxAgent-ASR 1.6.3" -srcfolder build/bin/PerfxAgent-ASR.app PerfxAgent-ASR-1.6.3.dmg

# 签名 DMG
codesign --force --deep --sign "Developer ID Application: Your Name" PerfxAgent-ASR-1.6.3.dmg

# 公证 DMG
xcrun altool --notarize-app --primary-bundle-id "com.perfx.agent.asr" \
  --username "your-apple-id" --password "app-specific-password" \
  --file PerfxAgent-ASR-1.6.3.dmg
```

### 4. 发布检查清单

- [ ] 版本号已更新
- [ ] 应用程序已测试
- [ ] 代码已签名
- [ ] DMG 已公证
- [ ] 文档已更新
- [ ] 发布说明已准备

## 🎵 支持格式

### 音频格式
- **输入格式**: WAV, MP3, FLAC, OGG
- **输出格式**: WAV, Opus
- **采样率**: 8kHz - 48kHz
- **声道**: 单声道/立体声
- **编码**: PCM, Opus, MP3

### 转录格式
- **LRC 格式**: 标准歌词同步格式
- **JSON 格式**: 结构化数据格式
- **实时文本**: 支持中间结果和最终结果

## 📊 版本信息

- **当前版本**: 1.6.2
- **Qt 版本**: Qt6
- **C++ 标准**: C++17
- **支持平台**: macOS (主要), Linux, Windows (计划中)
- **许可证**: MIT

## 📚 参考资料

- [CMake CPack 文档](https://cmake.org/cmake/help/latest/module/CPack.html)
- [Qt for macOS 部署指南](https://doc.qt.io/qt-6/macos-deployment.html)
- [Apple 代码签名指南](https://developer.apple.com/support/code-signing/)
- [macOS 公证指南](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution)
- [IXWebSocket 文档](https://github.com/machinezone/IXWebSocket)
- [PortAudio 文档](http://www.portaudio.com/docs/)
- [Opus 音频编解码器](https://opus-codec.org/) 