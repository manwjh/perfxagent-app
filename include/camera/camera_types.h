#pragma once

#include <string>
#include <vector>
#include <memory>
#include <opencv2/opencv.hpp>

namespace perfx {

struct CameraDeviceInfo {
    std::string deviceId;
    std::string name;
    std::string description;
    bool isDefault;
    std::vector<cv::Size> supportedResolutions;
    std::vector<double> supportedFrameRates;
};

struct CameraConfig {
    int deviceIndex;
    cv::Size resolution;
    double frameRate;
    std::string format;
    bool autoFocus;
    bool autoExposure;
};

struct CameraFrame {
    cv::Mat image;
    int64_t timestamp;
    bool isKeyFrame;
};

enum class CameraError {
    NONE = 0,
    DEVICE_NOT_FOUND,
    DEVICE_ALREADY_OPEN,
    DEVICE_NOT_OPEN,
    OPEN_FAILED,
    CLOSE_FAILED,
    CAPTURE_FAILED,
    ROTATE_FAILED,
    INVALID_PARAMETER
};

} // namespace perfx 