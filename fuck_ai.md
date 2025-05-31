cursor在帮助我修改、调试代码时，必须严格遵守如下规则！

## freeze如下模块结构，如果需要修改必须征求我的同意，并且非常严谨。

### Audio流式采集和播放模块
#### 系统架构
底层：PortAudio 接口
中层：AudioDevice 实现
上层：AudioProcessor 和 AudioManager
#### 目录结构
perfxagent-app/
├── include/
│   └── audio/
│       ├── audio_types.h      # 基础类型定义
│       ├── audio_device.h     # 音频设备接口
│       ├── audio_processor.h  # 音频处理器接口
│       ├── audio_thread.h     # 音频流线程接口
│       └── audio_manager.h    # 音频管理器接口
│
└── src/
    └── audio/
        ├── audio_device.cpp      # 音频设备实现
        ├── audio_processor.cpp   # 音频处理实现
        ├── audio_manager.cpp     # 音频模块管理器实现
        ├── audio_thread.cpp      # 音频流线程实现
        └── examples/             # 示例




## freeze如下模块代码，如果需要修改必须征求我的同意，并且非常严谨。
