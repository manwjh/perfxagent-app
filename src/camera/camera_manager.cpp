#include "camera/camera_manager.h"
#include "camera/camera_device.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <sstream>

namespace perfx {

// CameraStream 实现
CameraStream::CameraStream(std::shared_ptr<CameraDevice> device)
    : device_(device) {
}

bool CameraStream::readFrame(CameraFrame& frame) {
    if (!device_ || !device_->isOpen()) {
        return false;
    }
    return device_->captureFrame(frame);
}

bool CameraStream::isOpen() const {
    return device_ && device_->isOpen();
}

void CameraStream::close() {
    if (device_) {
        device_->close();
    }
}

// CameraManager 实现
CameraManager& CameraManager::getInstance() {
    static CameraManager instance;
    return instance;
}

CameraManager::~CameraManager() {
    closeAllDevices();
}

std::vector<CameraDeviceInfo> CameraManager::enumerateDevices() {
    std::vector<CameraDeviceInfo> devices;
    // 使用 OpenCV 枚举摄像头
    cv::VideoCapture cap;
    for (int i = 0; i < 10; ++i) {  // 尝试前10个设备
        if (cap.open(i)) {
            CameraDeviceInfo info;
            info.deviceId = std::to_string(i);
            info.name = "Camera " + std::to_string(i);
            info.description = "Camera device " + std::to_string(i);
            info.isDefault = (i == 0);  // 假设第一个设备是默认设备

            // 获取支持的格式
            cv::Mat frame;
            if (cap.read(frame)) {
                info.supportedResolutions.push_back({frame.cols, frame.rows});
            }
            info.supportedFrameRates = {30.0, 60.0};  // 假设支持这些帧率

            devices.push_back(info);
            cap.release();
        }
    }
    return devices;
}

std::shared_ptr<CameraDevice> CameraManager::getDefaultDevice() {
    auto devices = enumerateDevices();
    if (devices.empty()) {
        lastError_ = CameraError::DEVICE_NOT_FOUND;
        return nullptr;
    }
    return getDevice(0);  // 返回第一个设备作为默认设备
}

std::shared_ptr<CameraDevice> CameraManager::getDevice(int deviceId) {
    // 如果设备已经打开，直接返回
    auto it = openDevices_.find(deviceId);
    if (it != openDevices_.end()) {
        return it->second;
    }

    // 创建新设备
    CameraDeviceInfo info;
    info.deviceId = std::to_string(deviceId);
    info.name = "Camera " + std::to_string(deviceId);
    info.description = "Camera device " + std::to_string(deviceId);
    info.isDefault = (deviceId == 0);
    info.supportedFrameRates = {30.0, 60.0};

    auto device = std::make_shared<OpenCVCameraDevice>(info);
    if (!device) {
        lastError_ = CameraError::DEVICE_NOT_FOUND;
        return nullptr;
    }

    return device;
}

bool CameraManager::openDevice(int deviceId, const CameraConfig& config) {
    if (isDeviceOpen(deviceId)) {
        lastError_ = CameraError::DEVICE_ALREADY_OPEN;
        return false;
    }

    auto device = getDevice(deviceId);
    if (!device) {
        lastError_ = CameraError::DEVICE_NOT_FOUND;
        return false;
    }

    if (!device->open(config)) {
        lastError_ = CameraError::OPEN_FAILED;
        return false;
    }

    openDevices_[deviceId] = device;
    return true;
}

bool CameraManager::closeDevice(int deviceId) {
    auto it = openDevices_.find(deviceId);
    if (it == openDevices_.end()) {
        lastError_ = CameraError::DEVICE_NOT_OPEN;
        return false;
    }

    it->second->close();
    openDevices_.erase(it);
    return true;
}

void CameraManager::closeAllDevices() {
    for (auto& pair : openDevices_) {
        pair.second->close();
    }
    openDevices_.clear();
}

bool CameraManager::isDeviceOpen(int deviceId) const {
    return openDevices_.find(deviceId) != openDevices_.end();
}

std::shared_ptr<CameraStream> CameraManager::getStream(int deviceId) {
    if (!isDeviceOpen(deviceId)) {
        lastError_ = CameraError::DEVICE_NOT_OPEN;
        return nullptr;
    }
    return std::make_shared<CameraStream>(openDevices_[deviceId]);
}

bool CameraManager::captureImage(int deviceId, const std::string& filename) {
    if (!isDeviceOpen(deviceId)) {
        lastError_ = CameraError::DEVICE_NOT_OPEN;
        return false;
    }

    CameraFrame frame;
    if (!openDevices_[deviceId]->captureFrame(frame)) {
        lastError_ = CameraError::CAPTURE_FAILED;
        return false;
    }

    if (!cv::imwrite(filename, frame.image)) {
        lastError_ = CameraError::CAPTURE_FAILED;
        return false;
    }

    return true;
}

bool CameraManager::captureImage(int deviceId, cv::Mat& image) {
    if (!isDeviceOpen(deviceId)) {
        lastError_ = CameraError::DEVICE_NOT_OPEN;
        return false;
    }

    CameraFrame frame;
    if (!openDevices_[deviceId]->captureFrame(frame)) {
        lastError_ = CameraError::CAPTURE_FAILED;
        return false;
    }

    frame.image.copyTo(image);
    return true;
}

bool CameraManager::rotateImage(int deviceId, int angle) {
    if (!isDeviceOpen(deviceId)) {
        lastError_ = CameraError::DEVICE_NOT_OPEN;
        return false;
    }

    CameraFrame frame;
    if (!openDevices_[deviceId]->captureFrame(frame)) {
        lastError_ = CameraError::CAPTURE_FAILED;
        return false;
    }

    cv::Mat rotated;
    if (!rotateImage(frame.image, rotated, angle)) {
        return false;
    }

    frame.image = rotated;
    return true;
}

bool CameraManager::rotateImage(const cv::Mat& input, cv::Mat& output, int angle) {
    if (input.empty()) {
        lastError_ = CameraError::INVALID_PARAMETER;
        return false;
    }

    try {
        cv::rotate(input, output, angle);
        return true;
    } catch (const cv::Exception& e) {
        lastError_ = CameraError::ROTATE_FAILED;
        return false;
    }
}

CameraError CameraManager::getLastError() const {
    return lastError_;
}

std::string CameraManager::getLastErrorString() const {
    switch (lastError_) {
        case CameraError::NONE:
            return "No error";
        case CameraError::DEVICE_NOT_FOUND:
            return "Device not found";
        case CameraError::DEVICE_ALREADY_OPEN:
            return "Device already open";
        case CameraError::DEVICE_NOT_OPEN:
            return "Device not open";
        case CameraError::OPEN_FAILED:
            return "Failed to open device";
        case CameraError::CLOSE_FAILED:
            return "Failed to close device";
        case CameraError::CAPTURE_FAILED:
            return "Failed to capture frame";
        case CameraError::ROTATE_FAILED:
            return "Failed to rotate image";
        case CameraError::INVALID_PARAMETER:
            return "Invalid parameter";
        default:
            return "Unknown error";
    }
}

} // namespace perfx 