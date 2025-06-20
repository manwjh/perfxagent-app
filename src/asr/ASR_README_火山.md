# ASR 语音识别实验项目

## 项目简介

这是一个基于C++的实时语音识别(ASR)实验项目，实现了与语音识别服务器的WebSocket通信协议，支持流式音频传输和实时识别结果返回。

## 主要特性

- 🔄 **流式语音识别**：支持实时音频流传输和识别
- 🌐 **WebSocket通信**：基于IXWebSocket库实现稳定的网络通信
- 📦 **音频分包处理**：自动将音频文件分割成适合传输的小包
- 🗜️ **Gzip压缩**：音频数据使用Gzip压缩减少传输量
- 🏗️ **分层架构**：用户层、业务层、通信层清晰分离
- ⚡ **实时响应**：支持服务器心跳检测和错误处理
- 📊 **详细日志**：提供完整的协议解析和调试信息

## 项目架构

### 分层设计

```
┌─────────────────┐
│   用户层 (User Layer)    │  ← 简化的API接口
├─────────────────┤
│  业务层 (Business Layer) │  ← 音频处理、分包、流式控制
├─────────────────┤
│ 通信层 (Communication)   │  ← WebSocket连接、协议处理
└─────────────────┘
```

### 核心组件

- **AsrManager**: 业务层管理器，负责音频处理和流式控制
- **AsrClient**: 通信层客户端，处理WebSocket连接和协议
- **AudioProcessor**: 音频处理工具，支持WAV文件解析
- **ProtocolHandler**: 协议处理器，负责二进制协议解析

## 技术栈

- **语言**: C++17
- **网络库**: IXWebSocket
- **JSON处理**: nlohmann/json
- **音频处理**: 自定义WAV解析器
- **压缩**: Gzip (zlib)
- **构建系统**: CMake

## 编译要求

### 系统要求
- macOS/Linux/Windows
- C++17兼容的编译器
- CMake 3.15+

### 依赖库
- IXWebSocket
- nlohmann/json
- zlib (用于Gzip压缩)

## 快速开始

### 1. 环境准备

```bash
# 克隆项目
git clone <repository-url>
cd perfxagent-app-1.0.1

# 创建构建目录
mkdir build && cd build

# 配置CMake
cmake ..
```

### 2. 编译项目

```bash
# 编译所有目标
make -j4

# 或者编译特定目标
make asr_simple_example
```

### 3. 运行示例

```bash
# 运行简单示例
./bin/asr_simple_example

# 运行完整示例
./bin/perfxagent-app
```

## 使用指南

### 基本使用

```cpp
#include "asr/asr_manager.h"

int main() {
    // 创建ASR管理器
    AsrManager manager;
    
    // 初始化连接
    if (!manager.initialize()) {
        std::cerr << "初始化失败" << std::endl;
        return -1;
    }
    
    // 识别音频文件
    std::string audioFile = "test.wav";
    if (manager.recognizeAudioFile(audioFile)) {
        std::cout << "识别成功" << std::endl;
    }
    
    return 0;
}
```

### 高级配置

```cpp
// 自定义配置
AsrConfig config;
config.serverUrl = "wss://your-asr-server.com";
config.token = "your-access-token";
config.sampleRate = 16000;
config.channels = 1;

AsrManager manager(config);
```

## 协议说明

### 通信协议

项目实现了自定义的二进制协议，包含以下组件：

1. **协议头** (8字节)
   - 版本号 (1字节)
   - 头部大小 (1字节)
   - 消息类型 (1字节)
   - 消息标志 (1字节)
   - 序列化方法 (1字节)
   - 压缩类型 (1字节)
   - 保留字段 (1字节)
   - 序列号 (1字节)

2. **序列号** (4字节，大端序)
3. **Payload大小** (4字节，大端序)
4. **Payload数据** (Gzip压缩)

### 消息类型

- `AUDIO_ONLY_REQUEST`: 音频数据请求
- `FULL_SERVER_RESPONSE`: 服务器完整响应
- `PING`: 心跳检测

## 流式处理流程

```
1. 连接建立
   ↓
2. 音频文件读取
   ↓
3. 音频分包 (100ms/包)
   ↓
4. 发送第一个音频包
   ↓
5. 等待服务器响应
   ↓
6. 收到响应后发送下一包
   ↓
7. 重复步骤5-6直到所有包发送完成
   ↓
8. 等待最终识别结果
```

## 错误处理

### 常见错误类型

- **连接错误**: 网络连接失败
- **协议错误**: 数据格式不匹配
- **音频错误**: 音频格式不支持
- **超时错误**: 服务器响应超时

### 错误处理策略

```cpp
// 检查连接状态
if (!manager.isConnected()) {
    std::cerr << "连接已断开" << std::endl;
}

// 获取最后错误
auto error = manager.getLastError();
if (!error.isSuccess()) {
    std::cerr << "错误: " << error.message << std::endl;
}
```

## 性能优化

### 音频分包策略

- **包大小**: 根据采样率和时长计算，默认100ms
- **压缩**: 使用Gzip压缩减少传输量
- **并发**: 支持异步发送和接收

### 内存管理

- **智能指针**: 使用智能指针管理资源
- **缓冲区**: 预分配缓冲区减少内存分配
- **流式处理**: 避免一次性加载大文件

## 调试和日志

### 日志级别

项目提供详细的调试信息：

```bash
# 协议解析日志
🔍 协议解析:
  - 协议版本: 1
  - 头部大小: 1 (4字节块)
  - 消息类型: 9
  - payload size: 741

# 音频处理日志
✅ 音频分包完成: 377 个包
📤 发送音频包，剩余 376 个包

# 识别结果日志
🧹 解析后的响应: {"audio_info":{"duration":4297},"result":{...}}
```

### 调试模式

```cpp
// 启用详细日志
manager.setDebugMode(true);

// 查看协议数据
manager.setProtocolLogging(true);
```

## 示例文件

### 简单示例 (`examples/asr_simple_example.cpp`)

演示基本的音频识别功能：

```cpp
int main() {
    AsrManager manager;
    
    if (!manager.initialize()) {
        return -1;
    }
    
    // 识别测试音频文件
    manager.recognizeAudioFile("test_audio.wav");
    
    return 0;
}
```

### 完整示例 (`examples/perfxagent-app.cpp`)

包含完整的GUI界面和高级功能。

## 测试

### 单元测试

```bash
# 运行单元测试
make test
./bin/asr_tests
```

### 集成测试

```bash
# 运行集成测试
make integration_test
./bin/integration_test
```

## 贡献指南

### 开发环境设置

1. 安装依赖库
2. 配置IDE (推荐使用CLion或VS Code)
3. 设置代码格式化规则

### 代码规范

- 使用C++17标准
- 遵循Google C++ Style Guide
- 添加适当的注释和文档
- 编写单元测试

### 提交规范

```
feat: 添加新功能
fix: 修复bug
docs: 更新文档
style: 代码格式调整
refactor: 代码重构
test: 添加测试
chore: 构建过程或辅助工具的变动
```

## 许可证

本项目采用 MIT 许可证 - 查看 [LICENSE](LICENSE) 文件了解详情。

## 联系方式

- 项目维护者: [Your Name]
- 邮箱: [your.email@example.com]
- 项目地址: [GitHub Repository URL]

## 更新日志

### v1.0.1 (2024-01-XX)
- ✅ 实现流式语音识别
- ✅ 修复协议解析问题
- ✅ 优化音频分包策略
- ✅ 添加详细错误处理
- ✅ 完善日志系统

### v1.0.0 (2024-01-XX)
- 🎉 初始版本发布
- ✅ 基础WebSocket通信
- ✅ 音频文件处理
- ✅ 简单识别功能

---

**注意**: 这是一个实验性项目，主要用于学习和研究目的。在生产环境中使用前，请确保充分测试和验证。
