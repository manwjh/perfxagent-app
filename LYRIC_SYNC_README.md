# 歌词同步格式功能说明

## 概述

在 `AudioManager` 中新增了歌词同步格式功能，用于将ASR转录的文本整理成带时间戳的歌词同步格式，供上层应用获取和使用。

## 功能特性

### 1. 数据结构

#### LyricSegment（歌词片段）
```cpp
struct LyricSegment {
    std::string text;           // 文本内容
    double startTime;           // 开始时间（毫秒）
    double endTime;             // 结束时间（毫秒）
    double confidence;          // 置信度
    bool isFinal;               // 是否为最终结果
};
```

#### LyricSyncManager（歌词同步管理器）
- 管理歌词片段列表
- 维护完整文本
- 提供线程安全的操作
- 支持多种导出格式

### 2. 主要功能

#### ASR结果解析
- 自动解析火山引擎ASR返回的JSON结果
- 提取单词级别的时间戳信息
- 支持中间结果和最终结果处理

#### 歌词查询
- 根据时间点获取当前歌词
- 获取所有歌词片段
- 获取完整转录文本

#### 格式导出
- **LRC格式**：标准的歌词文件格式，支持大多数播放器
- **JSON格式**：结构化数据，便于程序处理

#### 文件保存
- 支持保存为LRC文件
- 支持保存为JSON文件
- 自动处理文件路径和权限

## 使用方法

### 1. 基本使用

```cpp
#include "audio/audio_manager.h"

// 获取AudioManager实例
AudioManager& audioManager = AudioManager::getInstance();

// 从ASR结果更新歌词同步数据
std::string asrResult = "..."; // ASR返回的JSON字符串
audioManager.updateLyricSyncFromASR(asrResult);

// 获取指定时间点的歌词
std::string currentLyric = audioManager.getCurrentLyric(1500.0); // 1500ms

// 获取完整转录文本
std::string fullText = audioManager.getFullTranscriptionText();
```

### 2. 导出功能

```cpp
// 导出为LRC格式
std::string lrcContent = audioManager.exportLyricsToLRC();

// 导出为JSON格式
std::string jsonContent = audioManager.exportLyricsToJSON();

// 保存到文件
audioManager.saveLyricsToFile("output.lrc", "lrc");
audioManager.saveLyricsToFile("output.json", "json");
```

### 3. 信号和槽

```cpp
// 连接歌词更新信号
connect(&audioManager, &AudioManager::lyricUpdated, 
        this, &MyClass::onLyricUpdated);

// 处理歌词更新
void MyClass::onLyricUpdated(const QString& lyric, double timeMs) {
    // 更新UI显示
    updateLyricDisplay(lyric, timeMs);
}
```

## ASR结果格式支持

### 火山引擎ASR完整结果格式
```json
{
    "result": {
        "utterances": [
            {
                "definite": true,
                "end_time": 4100,
                "start_time": 1480,
                "text": "完整文本",
                "words": [
                    {
                        "end_time": 1560,
                        "start_time": 1480,
                        "text": "哎"
                    }
                ]
            }
        ]
    }
}
```

### 中间结果格式
```json
{
    "result": {
        "text": "中间识别结果",
        "confidence": 0.85
    }
}
```

## 示例输出

### LRC格式输出
```
[ti:ASR转录结果]
[ar:自动语音识别]
[al:PerfXAgent]
[by:ASR转录]

[00:01.48]哎
[00:01.72]对
[00:01.88]我们
[00:02.20]去
[00:02.36]之前
[00:02.60]去
[00:02.84]玩
[00:03.24]照片
[00:03.64]拍
[00:03.80]的
[00:04.10]漂亮
```

### JSON格式输出
```json
{
  "fullText": "哎 对 我们 去 之前 去 玩 照片 拍 的 漂亮",
  "totalDuration": 4100,
  "segments": [
    {
      "text": "哎",
      "startTime": 1480,
      "endTime": 1560,
      "confidence": 0.95,
      "isFinal": true
    }
  ]
}
```

## 构建和运行

### 编译示例
```bash
cd build
make lyric_sync_example
```

### 运行示例
```bash
./bin/lyric_sync_example
```

## 注意事项

1. **线程安全**：所有歌词同步操作都是线程安全的
2. **内存管理**：歌词数据会在AudioManager生命周期内保持
3. **错误处理**：所有方法都有适当的错误处理和日志输出
4. **性能考虑**：大量歌词片段时，查询操作使用线性搜索，适合一般使用场景

## 扩展功能

### 自定义歌词片段
```cpp
LyricSegment segment;
segment.text = "自定义文本";
segment.startTime = 1000.0;
segment.endTime = 2000.0;
segment.confidence = 0.9;
segment.isFinal = true;

audioManager.addLyricSegment(segment);
```

### 清空数据
```cpp
audioManager.clearLyrics();
```

## 版本历史

- **v1.5.1**：当前版本，支持基本的歌词同步格式功能
- 支持火山引擎ASR结果解析
- 支持LRC和JSON格式导出
- 提供完整的API接口和示例代码 