#include "camera/camera_manager.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <opencv2/opencv.hpp>
#include <QApplication>
#include <QMainWindow>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QPushButton>

using namespace perfx;

void printDeviceInfo(const CameraDeviceInfo& info) {
    std::cout << "Device ID: " << info.deviceId << std::endl;
    std::cout << "Name: " << info.name << std::endl;
    std::cout << "Description: " << info.description << std::endl;
    std::cout << "Is Default: " << (info.isDefault ? "Yes" : "No") << std::endl;
    std::cout << "Supported Resolutions:" << std::endl;
    for (const auto& res : info.supportedResolutions) {
        std::cout << "  " << res.width << "x" << res.height << std::endl;
    }
    std::cout << "Supported Frame Rates:" << std::endl;
    for (const auto& rate : info.supportedFrameRates) {
        std::cout << "  " << rate << " fps" << std::endl;
    }
    std::cout << std::endl;
}

// 将OpenCV的Mat转换为Qt的QImage
QImage mat2QImage(const cv::Mat& mat) {
    if(mat.type() == CV_8UC3) {
        cv::Mat rgb;
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
        return QImage((uchar*)rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    }
    return QImage();
}

int main(int argc, char *argv[]) {
    // Initialize Qt application
    QApplication app(argc, argv);

    // Create main window
    QMainWindow window;
    window.setWindowTitle("Camera Preview");
    window.resize(1280, 720);

    // Create central widget and layout
    QWidget* centralWidget = new QWidget(&window);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    window.setCentralWidget(centralWidget);

    // Create image label
    QLabel* imageLabel = new QLabel(centralWidget);
    imageLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(imageLabel);

    // Create buttons
    QPushButton* captureButton = new QPushButton("拍照", centralWidget);
    QPushButton* rotateButton = new QPushButton("旋转90°", centralWidget);
    layout->addWidget(captureButton);
    layout->addWidget(rotateButton);

    // Get camera manager instance
    auto& manager = CameraManager::getInstance();

    // Enumerate available cameras
    std::cout << "Enumerating cameras..." << std::endl;
    auto devices = manager.enumerateDevices();
    
    if (devices.empty()) {
        std::cout << "No cameras found!" << std::endl;
        return 1;
    }

    // Print information about each camera
    std::cout << "Found " << devices.size() << " camera(s):" << std::endl;
    for (const auto& device : devices) {
        printDeviceInfo(device);
    }

    // Configure and open the default camera
    CameraConfig config;
    config.deviceIndex = 0;
    config.resolution = cv::Size(1280, 720);
    config.frameRate = 30.0;
    config.format = "MJPG";
    config.autoFocus = true;
    config.autoExposure = true;

    if (!manager.openDevice(0, config)) {
        std::cout << "Failed to open camera: " << manager.getLastErrorString() << std::endl;
        return 1;
    }

    // Get camera stream
    auto stream = manager.getStream(0);
    if (!stream) {
        std::cout << "Failed to get camera stream: " << manager.getLastErrorString() << std::endl;
        return 1;
    }

    // 用于保存最新帧
    std::shared_ptr<cv::Mat> latestFrame = std::make_shared<cv::Mat>();
    std::mutex frameMutex;

    // Create a timer to read frames
    QTimer frameTimer;
    frameTimer.setInterval(33); // ~30 fps
    QObject::connect(&frameTimer, &QTimer::timeout, [&]() {
        CameraFrame frame;
        if (stream->readFrame(frame)) {
            QImage image = mat2QImage(frame.image);
            if (!image.isNull()) {
                imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
                    imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            // 保存最新帧
            {
                std::lock_guard<std::mutex> lock(frameMutex);
                frame.image.copyTo(*latestFrame);
            }
        }
    });
    frameTimer.start();

    // 拍照按钮点击事件
    QObject::connect(captureButton, &QPushButton::clicked, [&]() {
        std::string filename = "capture_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) + ".jpg";
        
        if (manager.captureImage(0, filename)) {
            std::cout << "Frame saved to " << filename << std::endl;
        } else {
            std::cout << "Failed to save frame: " << manager.getLastErrorString() << std::endl;
        }
    });

    // 旋转按钮点击事件
    QObject::connect(rotateButton, &QPushButton::clicked, [&]() {
        if (manager.rotateImage(0, 90)) {
            std::cout << "Image rotated successfully" << std::endl;
        } else {
            std::cout << "Failed to rotate image: " << manager.getLastErrorString() << std::endl;
        }
    });

    // Create a timer to check for key presses
    QTimer keyTimer;
    keyTimer.setInterval(100); // Check every 100ms
    QObject::connect(&keyTimer, &QTimer::timeout, [&]() {
        if (window.isActiveWindow()) {
            if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
                if (QApplication::keyboardModifiers() & Qt::Key_Q) {
                    app.quit();
                }
            }
        }
    });
    keyTimer.start();

    // Show window
    window.show();

    // Run Qt event loop
    int result = app.exec();

    // Cleanup
    manager.closeDevice(0);

    return result;
} 