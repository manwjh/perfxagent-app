# 开发指南 / Development Guide

## 🛠️ 开发环境设置 / Development Environment Setup

### 必需工具 / Required Tools

1. **编译器 / Compiler**
   ```bash
   # macOS
   xcode-select --install
   ```

2. **包管理器 / Package Manager**
   ```bash
   # 安装Homebrew
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **依赖库 / Dependencies**
   ```bash
   brew install qt@6 openssl@3 portaudio boost opus libogg nlohmann-json websocketpp asio opencv libsndfile
   ```

4. **可选工具 / Optional Tools**
   ```bash
   # 更好的图标处理
   brew install imagemagick
   
   # 代码格式化
   brew install clang-format
   ```

## 🚀 快速开发流程 / Quick Development Workflow

### 开发模式构建（推荐） / Development Build (Recommended)

```bash
# 一键开发模式构建，自动启用所有调试功能
./scripts/build_dev.sh
```

这个脚本会自动：
- 检查所有依赖
- 清理构建目录
- 以Debug模式构建项目
- 设置调试环境变量
- 可选择直接运行应用程序

### 调试模式构建 / Debug Build

```bash
# 构建调试版本并启用所有调试功能
./scripts/build_package.sh --debug
```

### 生产模式构建 / Production Build

```bash
# 构建生产版本并创建安装包
./scripts/build_package.sh
```

## 📁 项目结构说明 / Project Structure Explanation

### 核心目录 / Core Directories

- `include/` - 头文件，定义接口和数据结构
- `src/` - 源代码实现
- `resources/` - UI资源文件（图标、图片、字体）
- `scripts/` - 工具脚本
- `third_party/` - 第三方库
- `cmake/` - CMake配置文件

### 模块组织 / Module Organization

```
include/
├── asr/          # 语音识别模块
├── audio/        # 音频处理模块  
├── logic/        # 业务逻辑模块
└── ui/           # 用户界面模块
```

## 🎨 资源管理 / Resource Management

### 图标生成 / Icon Generation

1. **准备源文件**
   ```bash
   # 将您的LOGO文件放到指定位置
   cp your_logo.png resources/icons/PerfxAgent-ASR.png
   ```

2. **生成图标**
   ```bash
   ./scripts/icon_generator.sh
   ```

3. **使用图标**
   ```cpp
   // 在代码中引用图标
   std::string iconPath = "resources/icons/app_icon.ico";
   ```

### 资源命名规范 / Resource Naming Convention

- 文件名：小写字母，下划线分隔
- 尺寸：在文件名中包含尺寸信息
- 示例：`play_button_32x32.png`, `record_active.png`

## 📝 代码规范 / Code Standards

### C++代码规范 / C++ Code Standards

1. **命名约定 / Naming Convention**
   ```cpp
   // 类名：PascalCase
   class AudioManager {};
   
   // 函数名：camelCase
   void processAudioData();
   
   // 常量：UPPER_SNAKE_CASE
   const int MAX_BUFFER_SIZE = 1024;
   
   // 成员变量：m_camelCase
   class MyClass {
   private:
       int m_memberVariable;
   };
   ```

2. **文件组织 / File Organization**
   ```cpp
   // 头文件结构
   #pragma once
   
   // 系统头文件
   #include <string>
   #include <vector>
   
   // 第三方库头文件
   #include <QtWidgets>
   
   // 项目头文件
   #include "audio_manager.h"
   
   // 类声明
   class MyClass {
   public:
       MyClass();
       ~MyClass();
       
   private:
       // 私有成员
   };
   ```

3. **注释规范 / Comment Standards**
   ```cpp
   /**
    * @brief 音频处理管理器
    * 
    * 负责音频录制、播放和格式转换的核心类
    * 
    * @author Your Name
    * @date 2024-01-01
    */
   class AudioManager {
   public:
       /**
        * @brief 开始音频录制
        * @param deviceId 音频设备ID
        * @param sampleRate 采样率
        * @return 是否成功启动
        */
       bool startRecording(const std::string& deviceId, int sampleRate);
   };
   ```

### 提交规范 / Commit Standards

```bash
# 提交格式
<type>(<scope>): <subject>

# 类型说明
feat:     新功能
fix:      修复bug
docs:     文档更新
style:    代码格式调整
refactor: 重构
test:     测试相关
chore:    构建过程或辅助工具的变动

# 示例
feat(audio): 添加音频录制功能
fix(asr): 修复ASR连接超时问题
docs(readme): 更新安装说明
```

## 🔧 开发工具 / Development Tools

### 推荐的IDE设置 / Recommended IDE Setup

1. **CLion / Visual Studio Code**
   - 安装C++扩展
   - 配置clang-format
   - 设置CMake配置

2. **Qt Creator**
   - 配置Qt6路径
   - 设置调试器
   - 配置版本控制

### 调试技巧 / Debugging Tips

1. **启用ASR调试**
   ```bash
   # 使用开发模式构建脚本（推荐）
   ./scripts/build_dev.sh
   
   # 或手动设置环境变量
   export ASR_DEBUG=1
   export ASR_ENABLE_BUSINESS_LOG=1
   export ASR_ENABLE_FLOW_LOG=1
   export ASR_ENABLE_DATA_LOG=1
   ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR
   ```

2. **音频设备调试**
   ```cpp
   // 在代码中添加调试输出
   qDebug() << "Audio devices:" << audioManager.getDeviceList();
   ```

3. **内存泄漏检查**
   ```bash
   # 使用Valgrind (Linux)
   valgrind --leak-check=full ./perfxagent-app
   
   # 使用Instruments (macOS)
   instruments -t Leaks ./perfxagent-app
   ```

## 🚀 构建和测试 / Build and Test

### 构建流程 / Build Process

#### 开发模式（推荐）
```bash
# 使用开发模式构建脚本
./scripts/build_dev.sh
```

#### 手动构建
```bash
# 1. 清理构建目录
rm -rf build && mkdir build && cd build

# 2. 配置项目
cmake .. -DCMAKE_BUILD_TYPE=Debug

# 3. 编译
make -j$(sysctl -n hw.ncpu)

# 4. 运行测试
make test
```

### 测试策略 / Testing Strategy

1. **单元测试**
   - 使用Google Test或Catch2
   - 测试核心业务逻辑
   - 测试音频处理功能

2. **集成测试**
   - 测试ASR服务集成
   - 测试UI组件交互
   - 测试文件导入导出

3. **性能测试**
   - 音频处理性能
   - 内存使用情况
   - 网络连接稳定性

## 📦 发布准备 / Release Preparation

### 版本管理 / Version Management

1. **更新版本号**
   ```cpp
   // 在CMakeLists.txt中
   set(PERFXAGENT_VERSION_MAJOR 1)
   set(PERFXAGENT_VERSION_MINOR 6)
   set(PERFXAGENT_VERSION_PATCH 2)
   ```

2. **更新文档**
   - README.md
   - CHANGELOG.md
   - API文档

3. **生成图标资源**
   ```bash
   ./scripts/icon_generator.sh
   ```

### 打包发布 / Packaging

```bash
# 使用一键打包脚本
./scripts/build_package.sh

# 或手动创建macOS应用包
cd build
cpack -G "DragNDrop;TGZ"
cd ..
```

## 🤝 贡献指南 / Contributing Guidelines

### 提交流程 / Submission Process

1. Fork项目
2. 创建功能分支
3. 编写代码和测试
4. 提交Pull Request
5. 代码审查
6. 合并到主分支

### 代码审查要点 / Code Review Points

- 代码质量和可读性
- 性能影响
- 安全性考虑
- 向后兼容性
- 文档完整性

## 📞 获取帮助 / Getting Help

- 查看项目文档
- 检查Issues页面
- 联系项目维护者
- 参与社区讨论

---

**注意**: 本指南会随着项目发展持续更新，请定期查看最新版本。
**Note**: This guide will be continuously updated as the project evolves. Please check for the latest version regularly. 