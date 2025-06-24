# 构建指南 / Build Guide

## 🚀 快速构建 / Quick Build

### 开发模式（推荐） / Development Mode (Recommended)
```bash
# 一键开发模式构建，自动启用调试功能
./scripts/build_dev.sh
```

### 生产模式 / Production Mode
```bash
# 一键生产模式构建和打包
./scripts/build_package.sh
```

### 调试模式 / Debug Mode
```bash
# 构建调试版本并启用所有调试功能
./scripts/build_package.sh --debug
```

## 📋 系统要求 / System Requirements

### 必需工具 / Required Tools
- macOS 10.15+ (Catalina)
- Xcode Command Line Tools
- Homebrew
- CMake 3.10+
- 支持C++17的编译器 (Apple Clang 12+)

### 依赖库 / Dependencies
```bash
# 安装所有依赖
brew install qt@6 openssl@3 portaudio boost opus libogg nlohmann-json websocketpp asio opencv libsndfile
```

## 🔧 手动构建 / Manual Build

### 1. 开发模式 / Development Mode
```bash
# 清理并创建构建目录
rm -rf build && mkdir build && cd build

# 配置项目（Debug模式）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 编译项目
make -j$(sysctl -n hw.ncpu)

# 运行应用程序（启用调试模式）
cd ..
export ASR_DEBUG=1
export ASR_ENABLE_BUSINESS_LOG=1
export ASR_ENABLE_FLOW_LOG=1
export ASR_ENABLE_DATA_LOG=1
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

### 2. 生产模式 / Production Mode
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

## 🎨 图标生成 / Icon Generation

### 准备图标
```bash
# 1. 将您的LOGO文件放到指定位置
cp your_logo.png resources/icons/PerfxAgent-ASR.png

# 2. 运行图标生成脚本
./scripts/icon_generator.sh

# 3. 检查生成的图标文件
ls -la resources/icons/
```

### 生成的图标格式
- PNG格式：16x16 到 1024x1024 各种尺寸
- ICO格式：Windows应用图标
- ICNS格式：macOS应用图标

## 🔍 调试模式 / Debug Mode

### 环境变量 / Environment Variables
```bash
# ASR调试
export ASR_DEBUG=1

# 业务逻辑日志
export ASR_ENABLE_BUSINESS_LOG=1

# 流程控制日志
export ASR_ENABLE_FLOW_LOG=1

# 数据处理日志
export ASR_ENABLE_DATA_LOG=1

# 协议通信日志
export ASR_ENABLE_PROTOCOL_LOG=1

# 音频处理日志
export ASR_ENABLE_AUDIO_LOG=1
```

### 调试技巧 / Debug Tips
```bash
# 1. 使用开发模式构建脚本（推荐）
./scripts/build_dev.sh

# 2. 手动设置调试环境变量
export ASR_DEBUG=1
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR

# 3. 查看详细日志
./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR 2>&1 | tee debug.log
```

## 📦 打包发布 / Packaging

### 创建安装包
```bash
# 使用一键打包脚本
./scripts/build_package.sh

# 或手动创建安装包
cd build
cpack -G "DragNDrop;TGZ"
cd ..
```

### 安装包格式
- **DMG**: macOS拖拽安装包
- **TGZ**: 压缩包格式

### 验证安装包
```bash
# 检查生成的包文件
ls -la build/*.dmg build/*.tar.gz

# 挂载DMG进行测试
hdiutil attach build/PerfxAgent-ASR-1.6.2.dmg
/Volumes/PerfxAgent-ASR\ 1.6.2/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

## 🐛 故障排除 / Troubleshooting

### 常见问题 / Common Issues

#### 1. Qt6找不到 / Qt6 Not Found
```bash
# 确保Qt6已安装
brew install qt@6

# 设置Qt6路径
export Qt6_DIR="/opt/homebrew/lib/cmake/Qt6"
```

#### 2. 依赖库链接失败 / Dependency Linking Failed
```bash
# 检查库路径
otool -L build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR

# 修复库路径
install_name_tool -change "old_path" "new_path" build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

#### 3. CPack生成失败 / CPack Generation Failed
```bash
# 检查CMake配置
cmake --build build --target package

# 查看详细错误信息
cpack --debug --verbose
```

#### 4. 音频设备问题 / Audio Device Issues
- 检查系统音频权限
- 确认音频设备正常工作
- 查看音频设备列表

#### 5. ASR连接失败 / ASR Connection Failed
- 检查网络连接
- 验证ASR凭据
- 确认防火墙设置

### 调试工具 / Debug Tools
```bash
# 内存泄漏检查
instruments -t Leaks ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR

# 性能分析
instruments -t Time\ Profiler ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR

# 网络分析
instruments -t Network ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
```

## 📊 构建输出 / Build Output

### 成功构建后的文件 / Files After Successful Build
```
build/
├── bin/
│   └── PerfxAgent-ASR.app/          # macOS应用程序包
│       └── Contents/
│           ├── MacOS/
│           │   └── PerfxAgent-ASR   # 主可执行文件
│           ├── Resources/            # 资源文件
│           └── Info.plist           # 应用程序信息
├── lib/                             # 静态库文件
│   ├── libperfx_audio.a
│   ├── libperfx_asr_client.a
│   └── libperfx_asr_manager.a
├── PerfxAgent-ASR-1.6.2.dmg         # DMG安装包
└── PerfxAgent-ASR-1.6.2.tar.gz      # TGZ压缩包
```

## 🔗 相关文档 / Related Documentation

- [README.md](README.md) - 项目概述和快速开始
- [DEVELOPMENT.md](DEVELOPMENT.md) - 开发指南
- [PACKAGING_GUIDE.md](PACKAGING_GUIDE.md) - 打包指南
- [CHANGELOG.md](CHANGELOG.md) - 版本更新日志 