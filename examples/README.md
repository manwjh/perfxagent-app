# ASR 示例使用指南

本目录包含了PerfXAgent项目中ASR（自动语音识别）模块的使用示例。

## 编译

```bash
# 在项目根目录下
mkdir build && cd build
cmake ..
make

# 编译完成后，示例程序位于 build/bin/ 目录下
```

## 示例程序

### asr_usage_example

这是一个综合性的ASR使用示例，展示了不同的使用场景。

#### 基本用法

```bash
./bin/asr_usage_example <示例编号>
```

#### 示例说明

1. **示例1: 连接建立和Full Client Request流程展示**
   - 展示WebSocket连接建立过程
   - 展示Full Client Request的发送流程
   - 不发送音频数据，仅展示连接阶段

2. **示例5: 完整的音频文件识别流程**
   - 完整的音频文件识别流程
   - 包括连接、发送Full Client Request、发送音频、接收结果

#### 示例5详细用法

```bash
./bin/asr_usage_example 5 <appid> <token> <audio_file> [cluster]
```

**参数说明：**
- `appid`: 火山引擎ASR服务的应用ID
- `token`: 访问令牌
- `audio_file`: 要识别的音频文件路径（支持WAV格式）
- `cluster`: 可选，集群名称

**示例：**
```bash
# 使用硬编码的凭据
./bin/asr_usage_example 5 8388344882 vQWuOVrg*************ZfA5q2lG3 test.wav

# 使用自定义凭据
./bin/asr_usage_example 5 your_app_id your_token audio.wav your_cluster
```

## 工作流程展示

### 示例1: 连接建立和Full Client Request流程

运行示例1可以清楚地看到以下工作流程：

1. **阶段1: WebSocket连接建立**
   - WebSocket握手完成
   - 准备发送Full Client Request

2. **阶段2: Full Client Request发送**
   - 4字节消息头已构建
   - JSON参数已压缩
   - 二进制消息已发送

### 示例5: 完整的音频文件识别流程

运行示例5可以体验完整的识别流程：

1. **连接建立**
   - WebSocket连接
   - Full Client Request发送

2. **音频发送**
   - 音频文件分块读取
   - 二进制格式发送
   - GZIP压缩

3. **结果接收**
   - 实时接收中间结果
   - 接收最终识别结果

## 配置说明

### 硬编码凭据

示例中使用了硬编码的凭据（在asr_usage_example.cpp中定义）：

```cpp
#define ASR_APP_ID "8388344882"
#define ASR_ACCESS_TOKEN "vQWuOVrg*************ZfA5q2lG3"
#define ASR_SECRET_KEY "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e"
```

**注意：** 这些凭据仅用于演示，实际使用时请替换为您自己的凭据。

### 音频格式要求

- **格式**: WAV
- **采样率**: 16kHz
- **位深度**: 16bit
- **声道**: 单声道

## 依赖项

- Qt6 (Core, Network, WebSockets)
- zlib (GZIP压缩)
- nlohmann/json (JSON处理)

## 故障排除

### 常见问题

1. **连接失败**
   - 检查网络连接
   - 验证appid和token是否正确
   - 确认防火墙设置

2. **音频文件问题**
   - 确保音频文件存在且可读
   - 检查音频格式是否符合要求
   - 验证音频文件完整性

3. **编译错误**
   - 确保所有依赖库已正确安装
   - 检查CMake配置
   - 验证Qt6环境

### 调试信息

示例程序会输出详细的调试信息，包括：

- 连接状态
- 消息发送/接收
- 错误信息
- 识别结果

使用这些信息可以帮助诊断问题。

## 扩展

这些示例可以作为开发自己ASR应用的起点。主要扩展点：

1. **自定义回调处理**
   - 实现自己的AsrCallback类
   - 添加业务逻辑处理

2. **音频源集成**
   - 集成麦克风输入
   - 支持更多音频格式

3. **结果处理**
   - 保存识别结果
   - 实时显示
   - 与其他系统集成

## 参考

- [ASR Full Client Request实现文档](../docs/ASR_FULL_CLIENT_REQUEST.md)
- [火山引擎ASR API文档](https://www.volcengine.com/docs/82379) 