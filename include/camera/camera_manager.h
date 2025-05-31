#pragma once

#include <memory>
#include <vector>
#include <map>
#include <string>
#include <opencv2/opencv.hpp>
#include "camera/camera_device.h"
#include "camera/camera_types.h"

namespace perfx {

// 前向声明
class CameraStream;

// 流对象，用于获取摄像头帧
class CameraStream {
public:
    explicit CameraStream(std::shared_ptr<CameraDevice> device);
    bool readFrame(CameraFrame& frame);
    bool isOpen() const;
    void close();

private:
    std::shared_ptr<CameraDevice> device_;
};

class CameraManager {
public:
    static CameraManager& getInstance();

    // 禁止拷贝和赋值
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // 设备枚举
    std::vector<CameraDeviceInfo> enumerateDevices();
    std::shared_ptr<CameraDevice> getDefaultDevice();
    std::shared_ptr<CameraDevice> getDevice(int deviceId);

    // 设备生命周期管理
    bool openDevice(int deviceId, const CameraConfig& config);
    bool closeDevice(int deviceId);
    void closeAllDevices();
    bool isDeviceOpen(int deviceId) const;

    // 流操作
    std::shared_ptr<CameraStream> getStream(int deviceId);

    // 拍照功能
    bool captureImage(int deviceId, const std::string& filename);
    bool captureImage(int deviceId, cv::Mat& image);

    // 图像处理
    bool rotateImage(int deviceId, int angle);
    bool rotateImage(const cv::Mat& input, cv::Mat& output, int angle);

    // 错误处理
    CameraError getLastError() const;
    std::string getLastErrorString() const;

private:
    CameraManager() = default;
    ~CameraManager();

    std::map<int, std::shared_ptr<CameraDevice>> openDevices_;
    CameraError lastError_ = CameraError::NONE;
};

} // namespace perfx 