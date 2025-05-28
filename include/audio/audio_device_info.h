#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>

namespace perfx {

// 音频设备类型
enum class AudioDeviceType {
    INPUT,      // 输入设备（麦克风）
    OUTPUT,     // 输出设备（扬声器）
    DUPLEX      // 双工设备（同时支持输入输出）
};

// 音频格式
enum class AudioFormat {
    PCM_16BIT,  // 16位PCM
    PCM_24BIT,  // 24位PCM
    PCM_32BIT,  // 32位PCM
    FLOAT32     // 32位浮点
};

// 基础设备信息
struct BaseDeviceInfo {
    int32_t id;                     // 设备ID
    std::string name;               // 设备名称
    std::string driverName;         // 驱动名称
    bool isDefault;                 // 是否为默认设备
    bool isActive;                  // 设备是否处于活动状态
};

// 输入设备信息
struct InputDeviceInfo : public BaseDeviceInfo {
    int maxChannels;                // 最大通道数
    double defaultSampleRate;       // 默认采样率
    double sensitivity;             // 灵敏度
    double signalToNoiseRatio;      // 信噪比
    std::vector<double> supportedSampleRates; // 支持的采样率列表
};

// 输出设备信息
struct OutputDeviceInfo : public BaseDeviceInfo {
    int maxChannels;                // 最大通道数
    double defaultSampleRate;       // 默认采样率
    double maxVolume;               // 最大音量
    double impedance;               // 阻抗
    std::vector<double> supportedSampleRates; // 支持的采样率列表
};

} // namespace perfx 