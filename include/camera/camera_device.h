#pragma once

#include "camera_types.h"
#include <functional>
#include <memory>
#include <thread>

namespace perfx {

class CameraDevice {
public:
    using FrameCallback = std::function<void(const CameraFrame&)>;
    using ErrorCallback = std::function<void(CameraError)>;

    virtual ~CameraDevice() = default;

    // Device information
    virtual CameraDeviceInfo getDeviceInfo() const = 0;
    virtual bool isOpen() const = 0;

    // Device control
    virtual bool open(const CameraConfig& config) = 0;
    virtual void close() = 0;
    virtual bool startCapture() = 0;
    virtual void stopCapture() = 0;

    // Configuration
    virtual bool setConfig(const CameraConfig& config) = 0;
    virtual CameraConfig getConfig() const = 0;

    // Callbacks
    virtual void setFrameCallback(FrameCallback callback) = 0;
    virtual void setErrorCallback(ErrorCallback callback) = 0;

    // Capture
    virtual bool captureFrame(CameraFrame& frame) = 0;
    virtual bool saveFrame(const CameraFrame& frame, const std::string& filename) = 0;
};

class OpenCVCameraDevice : public CameraDevice {
public:
    OpenCVCameraDevice();
    OpenCVCameraDevice(const CameraDeviceInfo& deviceInfo);
    CameraDeviceInfo getDeviceInfo() const override;
    bool isOpen() const override;
    bool open(const CameraConfig& config) override;
    void close() override;
    bool startCapture() override;
    void stopCapture() override;
    bool setConfig(const CameraConfig& config) override;
    CameraConfig getConfig() const override;
    void setFrameCallback(FrameCallback callback) override;
    void setErrorCallback(ErrorCallback callback) override;
    bool captureFrame(CameraFrame& frame) override;
    bool saveFrame(const CameraFrame& frame, const std::string& filename) override;
private:
    CameraDeviceInfo deviceInfo_;
    CameraConfig config_;
    cv::VideoCapture camera_;
    bool isOpen_;
    bool isCapturing_;
    std::thread captureThread_;
    FrameCallback frameCallback_;
    ErrorCallback errorCallback_;
};

} // namespace perfx 