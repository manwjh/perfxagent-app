# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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