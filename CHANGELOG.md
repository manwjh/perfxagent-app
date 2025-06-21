# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.3.0] - 2024-12-19

### Added
- 完整的录音文件到ASR识别流程
  - 音频录制功能优化
  - 实时音频流处理
  - ASR语音识别集成
- 界面流式显示功能
  - 实时文字显示ASR识别结果
  - 音频包完整识别显示
  - 用户界面优化和现代化

### Changed
- 重构音频处理模块
- 优化ASR客户端连接稳定性
- 改进用户界面响应性能

### Fixed
- 修复音频录制中断问题
- 解决ASR识别结果显示延迟

## [1.1.0] - 2024-03-21

### Added
- 示例项目框架
  - 音频示例 (audio_example)
    - 音频设备管理
    - 音频录制功能
    - WAV/OPUS 编码支持
  - 相机示例 (camera_example)

### Changed
- 重构项目结构
  - 将音频核心功能封装为 perfx_audio 库
  - 优化 CMake 构建系统，支持示例项目自动编译
  - 统一项目依赖管理


### Dependencies
- Qt 6.4.0
- PortAudio 19.7.0
- Opus
- SAMPLERATE
- nlohmann_json 3.11.3
- GTest
- Boost 1.74.0
- OpenSSL 3.0.0


[1.0.0]: https://github.com/yourusername/perfxagent-app/releases/tag/v1.0.0 