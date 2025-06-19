# ASR模块使用说明

## 概述

ASR（Automatic Speech Recognition）模块是PerfXAgent项目中的语音识别组件，基于火山API的接口设计，使用Qt WebSocket实现。该模块提供了完整的语音识别功能，支持文件识别、流式识别和实时识别等多种模式，并实现了Token鉴权机制。

## 功能特性

- **多种识别模式**：支持文件识别、流式识别和实时识别
- **Token鉴权**：支持Bearer Token认证，符合火山API规范
- **Qt集成**：完全基于Qt框架，与项目其他组件无缝集成
- **线程安全**：支持多线程操作，提供线程安全的API
- **错误处理**：完善的错误处理和重连机制
- **回调机制**：灵活的回调接口，支持自定义处理逻辑
- **配置灵活**：支持多种音频格式和识别参数配置

## 架构设计

### 核心组件

1. **AsrClient**：底层ASR客户端，负责与火山API服务器的WebSocket通信
2. **AsrManager**：高级管理器，提供简化的API接口和线程管理
3. **AsrCallback**：回调接口，处理识别结果和状态变化

### 类图

```
AsrCallback (接口)
    ↑
AsrClient
    ↑
AsrManagerWorker
    ↑
AsrManager
```

## 安装和配置

### 依赖项

- Qt6 Core
- Qt6 Network
- Qt6 WebSockets
- nlohmann-json

### 编译配置

ASR模块已集成到项目的CMake构建系统中，编译时会自动包含以下组件：

- `perfx_asr` 静态库
- `asr_usage_example` 示例程序

## Token鉴权

### 鉴权格式

Token 鉴权
在连接建立时，须在发送的 GET 或 POST 请求中加上鉴权相关的 Authorization header。如下面示例：

GET /api/v2/asr HTTP/1.1
Host: openspeech.bytedance.com
Accept: */*
User-Agent: curl/7.54.0
Authorization: Bearer; FYaWxBiJnuh-0KBTS00KCo73rxmDnalivd1UDSD-W5E=
Authorization header 的格式是 Bearer; {token}

其中：
- `Bearer`：授权方法名
- `token`：控制台获取的访问令牌
- 授权方法名和token之间使用 `;` 分割

### 鉴权实现

在WebSocket连接建立时，会自动在HTTP请求头中添加：

```cpp
// 设置Authorization header: Bearer; {token}
QString authHeader = QString("Bearer; %1").arg(m_token);
request.setRawHeader("Authorization", authHeader.toUtf8());

// 设置其他必要的headers
request.setHeader(QNetworkRequest::UserAgentHeader, "PerfXAgent-ASR-Client/1.0");
request.setRawHeader("Accept", "*/*");
```

## 使用方法

### 1. 基本使用（带Token鉴权）

```cpp
#include "asr/asr_client.h"

// 创建ASR客户端
auto asrClient = std::make_unique<Asr::AsrClient>();

// 设置回调
class MyCallback : public Asr::AsrCallback {
    void onResult(Asr::AsrClient* client, const QString& text, bool isFinal) override {
        qDebug() << "识别结果:" << text << "最终:" << isFinal;
    }
    // ... 其他回调方法
};

auto callback = std::make_unique<MyCallback>();
asrClient->setCallback(callback.get());

// 配置客户端（包含鉴权信息）
asrClient->setAppId("8388344882");
asrClient->setToken("vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3");
asrClient->setSecretKey("oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e");
asrClient->setAuthType(Asr::AsrClient::TOKEN);
asrClient->setAudioFormat("wav", 1, 16000, 16);

// 连接并识别（自动进行Token鉴权）
if (asrClient->connect()) {
    asrClient->sendAudioFile("audio.wav");
}
```

### 2. 使用管理器

```cpp
#include "asr/asr_manager.h"

// 创建管理器
auto manager = std::make_unique<Asr::AsrManager>();

// 配置（包含鉴权信息）
manager->setAppId("8388344882");
manager->setToken("vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3");
manager->setAudioFormat("wav", 1, 16000, 16);

// 设置回调
manager->setResultCallback([](const QString& text, bool isFinal) {
    qDebug() << "结果:" << text << "最终:" << isFinal;
});

// 连接并识别（自动进行Token鉴权）
if (manager->connect()) {
    manager->recognizeFile("audio.wav");
}
```

### 3. 全局凭据定义

在示例程序中，可以使用全局宏定义凭据：

```cpp
// 全局凭据定义
#define ASR_APP_ID "8388344882"
#define ASR_ACCESS_TOKEN "vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
#define ASR_SECRET_KEY "oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e"

// 使用全局凭据
asrClient->setAppId(ASR_APP_ID);
asrClient->setToken(ASR_ACCESS_TOKEN);
asrClient->setSecretKey(ASR_SECRET_KEY);
```

### 4. 流式识别

```cpp
// 配置为流式模式
manager->setRecognitionMode(Asr::AsrManager::StreamMode);

// 分块发送音频数据
QByteArray audioChunk = getAudioChunk();
manager->recognizeAudioData(audioChunk, false); // 非最后一块

// 发送最后一块
manager->recognizeAudioData(lastChunk, true); // 最后一块
```

### 5. 实时识别

```cpp
// 配置为实时模式
manager->setRecognitionMode(Asr::AsrManager::RealtimeMode);

// 开始实时识别
if (manager->startRealtimeRecognition()) {
    // 在音频输入回调中发送数据
    void onAudioInput(const QByteArray& audioData) {
        manager->recognizeAudioData(audioData, false);
    }
}

// 停止识别
manager->stopRealtimeRecognition();
```

## API参考

### AsrClient

#### 配置方法

- `setAppId(const QString& appId)`：设置应用ID
- `setToken(const QString& token)`：设置访问令牌
- `setAuthType(AuthType type)`：设置认证类型（TOKEN/SIGNATURE）
- `setSecretKey(const QString& key)`：设置密钥（签名认证）
- `setAudioFormat(const QString& format, int channels, int sampleRate, int bits)`：设置音频格式
- `setCluster(const QString& cluster)`：设置集群

#### 连接控制

- `bool connect()`：连接到ASR服务器（自动进行Token鉴权）
- `void disconnect()`：断开连接
- `bool isConnected() const`：检查连接状态
- `ConnectionState getState() const`：获取连接状态

#### 音频发送

- `bool sendAudio(const QByteArray& audioData, bool isLast = false)`：发送音频数据
- `bool sendAudioFile(const QString& filePath)`：发送音频文件

### AsrManager

#### 配置方法

- `setAppId(const QString& appId)`：设置应用ID
- `setToken(const QString& token)`：设置访问令牌
- `setCluster(const QString& cluster)`：设置集群
- `setAudioFormat(const QString& format, int channels, int sampleRate, int bits)`：设置音频格式
- `setRecognitionMode(RecognitionMode mode)`：设置识别模式

#### 识别方法

- `bool recognizeFile(const QString& filePath)`：识别音频文件
- `bool recognizeAudioData(const QByteArray& audioData, bool isLast = false)`：识别音频数据
- `bool startRealtimeRecognition()`：开始实时识别
- `void stopRealtimeRecognition()`：停止实时识别

#### 回调设置

- `setResultCallback(std::function<void(const QString&, bool)> callback)`：设置结果回调
- `setErrorCallback(std::function<void(const QString&)> callback)`：设置错误回调

### AsrCallback

#### 回调方法

- `onOpen(AsrClient* client)`：连接建立时调用
- `onMessage(AsrClient* client, const QString& msg)`：收到消息时调用
- `onError(AsrClient* client, const QString& error)`：发生错误时调用
- `onClose(AsrClient* client)`：连接关闭时调用
- `onResult(AsrClient* client, const QString& text, bool isFinal)`：收到识别结果时调用

## 信号和槽

### AsrClient信号

- `connected()`：连接建立
- `disconnected()`：连接断开
- `error(const QString& error)`：发生错误
- `messageReceived(const QString& message)`：收到消息
- `resultReceived(const QString& text, bool isFinal)`：收到识别结果

### AsrManager信号

- `connected()`：连接建立
- `disconnected()`：连接断开
- `recognitionStarted()`：识别开始
- `recognitionFinished()`：识别完成
- `resultReceived(const QString& text, bool isFinal)`：收到识别结果
- `error(const QString& error)`：发生错误

## 错误处理

### 常见错误

1. **连接失败**：检查网络连接和服务器地址
2. **认证失败**：检查AppId和Token是否正确，确认Authorization header格式
3. **音频格式错误**：确保音频格式符合要求
4. **超时错误**：检查网络延迟和音频文件大小

### 错误处理示例

```cpp
manager->setErrorCallback([](const QString& error) {
    qWarning() << "ASR错误:" << error;
    
    if (error.contains("认证失败")) {
        // 重新配置认证信息
        manager->setToken("new_token");
        manager->connect();
    } else if (error.contains("网络错误")) {
        // 重试连接
        QTimer::singleShot(5000, [&]() {
            manager->connect();
        });
    }
});
```

## 性能优化

### 音频参数建议

- **采样率**：16kHz（推荐）或8kHz
- **声道数**：单声道（推荐）
- **位深度**：16位
- **格式**：WAV、PCM

### 流式识别优化

- **分块大小**：32KB（推荐）
- **发送间隔**：100ms（推荐）
- **缓冲区管理**：避免内存泄漏

### 线程安全

- 所有公共方法都是线程安全的
- 使用QMutex保护内部状态
- 信号和槽自动处理线程间通信

## 示例程序

项目包含完整的示例程序：

- **examples/asr_usage_example.cpp**：详细的使用示例，包含Token鉴权演示

### 运行示例

```bash
# 编译
cd build
make

# 运行详细示例（使用硬编码的凭据）
./bin/asr_usage_example 1
```

### 示例输出

```
ASR模块使用示例
================
使用硬编码的凭据:
  AppId: 8388344882
  Token: vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3
  SecretKey: oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e

=== 示例1: 仅连接和断开（带Token鉴权） ===
使用AppId: 8388344882
使用Token: vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3
使用SecretKey: oKzfTdLm0M2dVUXUKW86jb-hFLGPmG3e
Connecting to ASR server: "wss://openspeech.bytedance.com/api/v2/asr"
Authorization header: "Bearer; vQWuOVrgH6J0kCAQoHcQZ_wZfA5q2lG3"
示例执行完成
```

## 故障排除

### 编译问题

1. **找不到头文件**：确保CMakeLists.txt正确配置
2. **链接错误**：检查Qt6依赖是否正确安装
3. **MOC错误**：确保Q_OBJECT宏正确使用

### 运行时问题

1. **连接失败**：检查网络和防火墙设置
2. **认证失败**：检查Token格式和Authorization header
3. **识别失败**：检查音频格式和认证信息
4. **内存泄漏**：确保正确释放资源

## 更新日志

### v1.1.0
- 添加Token鉴权支持
- 实现Bearer Token认证机制
- 更新示例程序，使用硬编码凭据
- 优化连接流程和错误处理

### v1.0.0
- 初始版本
- 支持基本的ASR功能
- 集成Qt WebSocket
- 提供完整的API接口

## 许可证

本模块遵循项目的整体许可证条款。

## 贡献

欢迎提交问题报告和功能请求。请确保：

1. 遵循项目的代码规范
2. 添加适当的测试
3. 更新相关文档

## 联系方式

如有问题，请通过项目仓库提交Issue。 