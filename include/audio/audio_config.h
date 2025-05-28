#pragma once

#include "audio/audio_device_info.h"
#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>

namespace perfx {

// 音频配置结构
struct AudioConfig {
    // 基础配置
    int sampleRate = 16000;
    int channels = 1;
    AudioFormat format = AudioFormat::PCM_16BIT;
    int bufferSize = 1024;
    
    // 设备配置
    InputDeviceInfo inputDevice;
    OutputDeviceInfo outputDevice;
    
    // 录音配置
    QString recordingPath;  // 录音保存路径
    bool autoStartRecording; // 是否自动开始录音
    int maxRecordingDuration; // 最大录音时长（秒）
    
    // VAD配置
    float vadThreshold = 0.5f;

    // 获取默认输入设备配置
    static InputDeviceInfo getDefaultInputConfig() {
        InputDeviceInfo info;
        info.id = -1;
        info.name = "Default Input";
        info.maxChannels = 1;
        info.defaultSampleRate = 16000;
        info.sensitivity = 0.0;
        info.signalToNoiseRatio = 0.0;
        return info;
    }

    // 获取默认输出设备配置
    static OutputDeviceInfo getDefaultOutputConfig() {
        OutputDeviceInfo info;
        info.id = -1;
        info.name = "Default Output";
        info.maxChannels = 1;
        info.defaultSampleRate = 16000;
        info.maxVolume = 1.0;
        info.impedance = 0.0;
        return info;
    }

    QJsonObject toJson() const {
        QJsonObject obj;
        // 基础配置
        obj["sampleRate"] = sampleRate;
        obj["channels"] = channels;
        obj["format"] = static_cast<int>(format);
        obj["bufferSize"] = bufferSize;
        
        // 输入设备配置
        QJsonObject inputObj;
        inputObj["id"] = inputDevice.id;
        inputObj["name"] = QString::fromStdString(inputDevice.name);
        inputObj["maxChannels"] = inputDevice.maxChannels;
        inputObj["defaultSampleRate"] = inputDevice.defaultSampleRate;
        inputObj["sensitivity"] = inputDevice.sensitivity;
        inputObj["signalToNoiseRatio"] = inputDevice.signalToNoiseRatio;
        obj["inputDevice"] = inputObj;

        // 输出设备配置
        QJsonObject outputObj;
        outputObj["id"] = outputDevice.id;
        outputObj["name"] = QString::fromStdString(outputDevice.name);
        outputObj["maxChannels"] = outputDevice.maxChannels;
        outputObj["defaultSampleRate"] = outputDevice.defaultSampleRate;
        outputObj["maxVolume"] = outputDevice.maxVolume;
        outputObj["impedance"] = outputDevice.impedance;
        obj["outputDevice"] = outputObj;

        // 录音配置
        obj["recordingPath"] = recordingPath;
        obj["autoStartRecording"] = autoStartRecording;
        obj["maxRecordingDuration"] = maxRecordingDuration;
        
        // VAD配置
        obj["vadThreshold"] = vadThreshold;
        
        return obj;
    }

    void fromJson(const QJsonObject& obj) {
        // 基础配置
        sampleRate = obj["sampleRate"].toInt();
        channels = obj["channels"].toInt();
        format = static_cast<AudioFormat>(obj["format"].toInt());
        bufferSize = obj["bufferSize"].toInt();
        
        // 输入设备配置
        if (obj.contains("inputDevice")) {
            QJsonObject inputObj = obj["inputDevice"].toObject();
            inputDevice.id = inputObj["id"].toInt();
            inputDevice.name = inputObj["name"].toString().toStdString();
            inputDevice.maxChannels = inputObj["maxChannels"].toInt();
            inputDevice.defaultSampleRate = inputObj["defaultSampleRate"].toDouble();
            inputDevice.sensitivity = inputObj["sensitivity"].toDouble();
            inputDevice.signalToNoiseRatio = inputObj["signalToNoiseRatio"].toDouble();
        }

        // 输出设备配置
        if (obj.contains("outputDevice")) {
            QJsonObject outputObj = obj["outputDevice"].toObject();
            outputDevice.id = outputObj["id"].toInt();
            outputDevice.name = outputObj["name"].toString().toStdString();
            outputDevice.maxChannels = outputObj["maxChannels"].toInt();
            outputDevice.defaultSampleRate = outputObj["defaultSampleRate"].toDouble();
            outputDevice.maxVolume = outputObj["maxVolume"].toDouble();
            outputDevice.impedance = outputObj["impedance"].toDouble();
        }

        // 录音配置
        recordingPath = obj["recordingPath"].toString();
        autoStartRecording = obj["autoStartRecording"].toBool();
        maxRecordingDuration = obj["maxRecordingDuration"].toInt();
        
        // VAD配置
        vadThreshold = obj["vadThreshold"].toDouble();
    }
};

} // namespace perfx 