#include "camera/camera_device.h"
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

namespace perfx {

OpenCVCameraDevice::OpenCVCameraDevice()
    : isOpen_(false), isCapturing_(false) {}

OpenCVCameraDevice::OpenCVCameraDevice(const CameraDeviceInfo& deviceInfo)
    : deviceInfo_(deviceInfo), isOpen_(false), isCapturing_(false) {}

CameraDeviceInfo OpenCVCameraDevice::getDeviceInfo() const {
    return deviceInfo_;
}

bool OpenCVCameraDevice::isOpen() const {
    return isOpen_;
}

bool OpenCVCameraDevice::open(const CameraConfig& config) {
    if (isOpen_) {
        return true;
    }
    try {
        camera_.open(std::stoi(deviceInfo_.deviceId));
        if (!camera_.isOpened()) {
            if (errorCallback_) {
                errorCallback_(CameraError::DEVICE_NOT_FOUND);
            }
            return false;
        }
        camera_.set(cv::CAP_PROP_FRAME_WIDTH, config.resolution.width);
        camera_.set(cv::CAP_PROP_FRAME_HEIGHT, config.resolution.height);
        camera_.set(cv::CAP_PROP_FPS, config.frameRate);
        camera_.set(cv::CAP_PROP_AUTOFOCUS, config.autoFocus);
        camera_.set(cv::CAP_PROP_AUTO_EXPOSURE, config.autoExposure ? 1 : 0);
        config_ = config;
        isOpen_ = true;
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(CameraError::DEVICE_ALREADY_OPEN);
        }
        return false;
    }
}

void OpenCVCameraDevice::close() {
    if (isCapturing_) {
        stopCapture();
    }
    if (isOpen_) {
        camera_.release();
        isOpen_ = false;
    }
}

bool OpenCVCameraDevice::startCapture() {
    if (!isOpen_ || isCapturing_) {
        return false;
    }
    isCapturing_ = true;
    captureThread_ = std::thread([this]() {
        while (isCapturing_) {
            CameraFrame frame;
            if (captureFrame(frame)) {
                if (frameCallback_) {
                    frameCallback_(frame);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / static_cast<int>(config_.frameRate)));
        }
    });
    return true;
}

void OpenCVCameraDevice::stopCapture() {
    isCapturing_ = false;
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
}

bool OpenCVCameraDevice::setConfig(const CameraConfig& config) {
    if (!isOpen_) {
        return false;
    }
    try {
        camera_.set(cv::CAP_PROP_FRAME_WIDTH, config.resolution.width);
        camera_.set(cv::CAP_PROP_FRAME_HEIGHT, config.resolution.height);
        camera_.set(cv::CAP_PROP_FPS, config.frameRate);
        camera_.set(cv::CAP_PROP_AUTOFOCUS, config.autoFocus);
        camera_.set(cv::CAP_PROP_AUTO_EXPOSURE, config.autoExposure ? 1 : 0);
        config_ = config;
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(CameraError::INVALID_PARAMETER);
        }
        return false;
    }
}

CameraConfig OpenCVCameraDevice::getConfig() const {
    return config_;
}

void OpenCVCameraDevice::setFrameCallback(FrameCallback callback) {
    frameCallback_ = std::move(callback);
}

void OpenCVCameraDevice::setErrorCallback(ErrorCallback callback) {
    errorCallback_ = std::move(callback);
}

bool OpenCVCameraDevice::captureFrame(CameraFrame& frame) {
    if (!isOpen_) {
        return false;
    }
    try {
        cv::Mat image;
        if (!camera_.read(image)) {
            if (errorCallback_) {
                errorCallback_(CameraError::CAPTURE_FAILED);
            }
            return false;
        }
        frame.image = image;
        frame.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        frame.isKeyFrame = true;
        return true;
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(CameraError::CAPTURE_FAILED);
        }
        return false;
    }
}

bool OpenCVCameraDevice::saveFrame(const CameraFrame& frame, const std::string& filename) {
    try {
        std::vector<int> compression_params;
        compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
        compression_params.push_back(95); // High quality JPEG
        return cv::imwrite(filename, frame.image, compression_params);
    } catch (const std::exception& e) {
        if (errorCallback_) {
            errorCallback_(CameraError::CAPTURE_FAILED);
        }
        return false;
    }
}

} // namespace perfx 