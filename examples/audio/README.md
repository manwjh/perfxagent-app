# PerfX Audio Recording Tool

一个功能强大的音频录制工具，支持多种格式和智能配置管理。

## 🎯 主要特性

- **智能配置管理**: 自动保存和加载配置文件
- **多格式支持**: WAV（无损）和 OPUS（压缩）格式
- **专业音频参数**: 支持多种采样率、通道配置和编码设置
- **设备自动检测**: 智能识别和验证音频设备
- **用户友好界面**: 清晰的交互式配置流程

## 🚀 快速开始

### 首次运行

```bash
./audio_example
```

首次运行时，程序会自动进入交互式配置：

```
🎙️  PerfX Audio Recording Tool
================================

🔧 No configuration file found, entering interactive configuration...

=== Input Device Configuration ===
Available input devices:
0. MacBook Pro Microphone (channels: 1, sample rate: 48000Hz)
1. External USB Mic (channels: 2, sample rate: 44100Hz)

请选择输入设备 (0-1): 0
```

### 后续运行

再次运行时，如果存在配置文件，会提供快速启动选项：

```
🎙️  PerfX Audio Recording Tool
================================

📁 Found existing audio configuration file!
Do you want to:
1. Use existing configuration (quick start)
2. Enter interactive configuration

Enter your choice (1-2): 1

✅ Successfully loaded existing configuration!

📋 Configuration Summary:
  🎤 Device: MacBook Pro Microphone
  🎵 Sample Rate: 48000 Hz
  🔧 Encoding: OPUS
  📊 Bitrate: 32kbps
  💾 Output: recording_48000hz_mono.ogg
```

## ⚙️ 配置选项

### 音频设备
- 自动检测可用的输入设备
- 显示设备详细信息（通道数、采样率等）
- 智能验证设备可用性

### 音频参数
- **采样率**: 8000Hz - 48000Hz
- **通道**: 单声道/立体声
- **格式**: Float32/Int16
- **缓冲区大小**: 可配置帧数

### 编码格式

#### WAV格式
- 无损压缩
- 兼容性好
- 文件较大

#### OPUS格式（推荐）
- **帧长度**: 2.5ms - 60ms（基于libopus标准）
- **比特率**: 6kbps - 128kbps
- **复杂度**: 0-10（CPU使用 vs 质量平衡）
- **应用类型**: 
  - VOIP（语音通话优化）
  - Audio（音频流优化）
  - Restricted Low-delay（低延迟限制）

### VAD（语音活动检测）
- 可选启用/禁用
- 自定义阈值设置
- 静音检测和句子分割

## 📁 配置文件

配置文件 `audio_config.json` 自动保存在程序目录中，包含：

```json
{
    "audio": {
        "sampleRate": 48000,
        "channels": 1,
        "format": 3,
        "framesPerBuffer": 256
    },
    "device": {
        "name": "MacBook Pro Microphone",
        "index": 0,
        "type": 0,
        "maxInputChannels": 1,
        "maxOutputChannels": 0,
        "defaultSampleRate": 48000.0,
        "defaultLatency": 0.008
    },
    "encoding": {
        "format": 1,
        "opusFrameLength": 20,
        "opusBitrate": 32000,
        "opusComplexity": 5,
        "opusApplication": 2048
    },
    "metadata": {
        "version": "1.0",
        "timestamp": 1703123456,
        "description": "Audio recording configuration for perfxagent"
    }
}
```

## 🎤 使用流程

1. **启动程序**
   - 检查现有配置
   - 选择快速启动或交互配置

2. **设备验证**
   - 自动验证配置中的设备是否可用
   - 如果设备不存在，提示重新选择

3. **开始录音**
   - 显示详细的录音参数
   - 按 Enter 键停止录音

4. **文件保存**
   - 自动保存到 `recordings/` 目录
   - 文件名包含参数信息
   - 更新配置时间戳

## 🔧 高级设置

### Opus 编码优化

- **语音录制推荐**:
  - 帧长度: 20ms
  - 比特率: 32kbps
  - 复杂度: 5-6
  - 应用类型: VOIP

- **音乐录制推荐**:
  - 帧长度: 2.5-5ms
  - 比特率: 64-128kbps
  - 复杂度: 8-10
  - 应用类型: Audio

### 性能调优

- **低延迟需求**: 选择较短的帧长度
- **高质量需求**: 增加比特率和复杂度
- **低CPU使用**: 降低复杂度设置

## 📝 输出文件

录音文件保存在 `recordings/` 目录中，文件名格式：
- WAV: `recording_48000hz_mono.wav`
- OPUS: `recording_48000hz_mono.ogg`

## 🐛 故障排除

### 配置文件损坏
如果配置文件损坏，删除 `audio_config.json` 重新创建：
```bash
rm audio_config.json
./audio_example
```

### 设备不可用
程序会自动检测设备变化并提示重新选择：
```
⚠️  Warning: Previously configured device not found!
Device: Old Microphone (index: 1)
Please select a new device:
```

### Opus 编码错误
确保使用支持的帧长度：
- 2.5ms, 5ms, 10ms, 20ms, 40ms, 60ms

## 📊 支持格式

| 格式 | 扩展名 | 压缩 | 质量 | 兼容性 |
|------|--------|------|------|--------|
| WAV  | .wav   | 无损 | 最高 | 优秀   |
| OPUS | .ogg   | 有损 | 很高 | 良好   |

---

**PerfX Audio Tool** - 专业的音频录制解决方案 🎙️ 