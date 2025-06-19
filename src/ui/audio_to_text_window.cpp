#include "ui/audio_to_text_window.h"
#include "audio/file_importer.h"
#include "audio/audio_converter.h"
#include "audio/audio_manager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QDateTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QProgressBar>
#include <QTextEdit>
#include <QPushButton>
#include <QMenu>
#include <thread>
#include <chrono>
#include <iostream>

namespace perfx {
namespace ui {

AudioToTextWindow::AudioToTextWindow(QWidget *parent)
    : QMainWindow(parent)
{
    std::cout << "[DEBUG] Initializing AudioToTextWindow..." << std::endl;
    
    fileImporter_ = std::make_unique<audio::FileImporter>();
    audioConverter_ = std::make_unique<audio::AudioConverter>();
    
    if (!audioConverter_) {
        std::cerr << "[ERROR] Failed to create AudioConverter" << std::endl;
        QMessageBox::critical(this, "错误", "音频转换器初始化失败");
        return;
    }
    
    std::cout << "[DEBUG] AudioConverter created successfully" << std::endl;
    setupUI();
    setupMenuBar();
    std::cout << "[DEBUG] AudioToTextWindow initialization completed" << std::endl;
}

AudioToTextWindow::~AudioToTextWindow() = default;

void AudioToTextWindow::setupUI()
{
    setWindowTitle("录音转文本");
    resize(800, 600);

    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // 添加文本编辑区域
    textEdit_ = new QTextEdit(this);
    textEdit_->setReadOnly(true);
    mainLayout->addWidget(textEdit_);

    // 添加进度条
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    progressBar_->setValue(0);
    mainLayout->addWidget(progressBar_);

    // 添加按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* importButton = new QPushButton("导入文件", this);
    QPushButton* convertButton = new QPushButton("转换", this);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(convertButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号和槽
    connect(importButton, &QPushButton::clicked, this, &AudioToTextWindow::importFile);
    connect(convertButton, &QPushButton::clicked, this, &AudioToTextWindow::convertAudio);

    // 连接信号
    connectSignals();
}

void AudioToTextWindow::setupMenuBar()
{
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 文件菜单
    QMenu* fileMenu = menuBar->addMenu("文件");
    QAction* importAction = fileMenu->addAction("导入文件");
    QAction* exitAction = fileMenu->addAction("退出");
    connect(importAction, &QAction::triggered, this, &AudioToTextWindow::importFile);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 设置菜单
    QMenu* settingsMenu = menuBar->addMenu("设置");
    QAction* apiKeyAction = settingsMenu->addAction("设置API密钥");
    connect(apiKeyAction, &QAction::triggered, this, &AudioToTextWindow::setApiKey);
}

void AudioToTextWindow::connectSignals() {
    // 确保 AudioManager 已经初始化
    try {
        auto& audioManager = audio::AudioManager::getInstance();
        
        // 文件信息输出使用队列连接，因为它可能来自工作线程
        bool success = connect(&audioManager, &audio::AudioManager::outputFileInfo,
                this, &AudioToTextWindow::onOutputFileInfo, Qt::QueuedConnection);
        if (!success) {
            std::cerr << "[ERROR] Failed to connect outputFileInfo signal" << std::endl;
        }
        
        // 进度更新使用直接连接，因为需要实时性
        success = connect(&audioManager, &audio::AudioManager::conversionProgress,
                this, &AudioToTextWindow::onConversionProgress, Qt::DirectConnection);
        if (!success) {
            std::cerr << "[ERROR] Failed to connect conversionProgress signal" << std::endl;
        }
        
        // 转换完成和错误信号使用队列连接，因为它们可能来自工作线程
        success = connect(&audioManager, &audio::AudioManager::conversionComplete,
                this, &AudioToTextWindow::onConversionComplete, Qt::QueuedConnection);
        if (!success) {
            std::cerr << "[ERROR] Failed to connect conversionComplete signal" << std::endl;
        }
        
        success = connect(&audioManager, &audio::AudioManager::error,
                this, &AudioToTextWindow::onError, Qt::QueuedConnection);
        if (!success) {
            std::cerr << "[ERROR] Failed to connect error signal" << std::endl;
        }
        
        std::cout << "[DEBUG] Signal connections established successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Failed to get AudioManager instance: " << e.what() << std::endl;
        QMessageBox::critical(this, "错误", "音频管理器初始化失败");
    }
}

void AudioToTextWindow::onOutputFileInfo(const QString& info) {
    textEdit_->append(info);
}

void AudioToTextWindow::onConversionProgress(int progress) {
    progressBar_->setValue(progress);
}

void AudioToTextWindow::onConversionComplete(const QString& outputFile) {
    textEdit_->append("转换完成: " + outputFile);
    progressBar_->setValue(100);
}

void AudioToTextWindow::onError(const QString& errorMessage) {
    textEdit_->append("错误: " + errorMessage);
}

void AudioToTextWindow::importFile()
{
    // 获取用户文档目录作为默认导入目录
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    
    // 选择要导入的文件
    QStringList fileNames = QFileDialog::getOpenFileNames(this,
        "选择音频文件", defaultDir, 
        "音频文件 (*.wav *.mp3 *.ogg *.flac *.m4a *.aac);;所有文件 (*.*)");
    
    if (fileNames.isEmpty()) {
        return;
    }

    // 清空之前的文件列表
    inputFiles_.clear();
    
    // 保存文件路径并创建工作目录
    for (const QString& fileName : fileNames) {
        std::string filePath = fileName.toStdString();
        inputFiles_.push_back(filePath);
        
        // 获取文件所在目录
        std::filesystem::path path(filePath);
        std::string parentDir = path.parent_path().string();
        std::string fileNameWithoutExt = path.stem().string();
        
        // 创建工作目录
        std::string workDir = (std::filesystem::path(parentDir) / fileNameWithoutExt).string();
        if (std::filesystem::create_directories(workDir)) {
            textEdit_->append(QString("\n=== 文件导入信息 ==="));
            textEdit_->append(QString("文件: %1").arg(fileName));
            textEdit_->append(QString("工作目录: %1").arg(QString::fromStdString(workDir)));
            textEdit_->append(QString("该目录将作为该文件的工作目录"));
        }
    }
}

void AudioToTextWindow::convertAudio()
{
    if (inputFiles_.empty()) {
        QMessageBox::warning(this, "警告", "请先导入音频文件");
        return;
    }

    if (!audioConverter_) {
        std::cerr << "[ERROR] AudioConverter is not initialized" << std::endl;
        QMessageBox::critical(this, "错误", "音频转换器未初始化");
        return;
    }

    textEdit_->append("\n=== 开始转换过程 ===");
    std::cout << "[DEBUG] Starting conversion process..." << std::endl;

    // 设置进度回调
    audioConverter_->setProgressCallback([this](const audio::ConversionProgress& progress) {
        std::cout << "[DEBUG] Progress callback triggered: " << (progress.progress * 100) << "%" << std::endl;
        
        // 更新进度条
        int progressValue = static_cast<int>(progress.progress * 100);
        progressBar_->setValue(progressValue);
        
        // 显示音频信息
        if (progress.progress == 0) {
            textEdit_->append("\n=== 音频文件信息 ===");
            textEdit_->append(QString("采样率: %1 Hz").arg(progress.sourceFormat.sampleRate));
            textEdit_->append(QString("声道数: %1").arg(static_cast<int>(progress.sourceFormat.channels)));
            textEdit_->append(QString("位深度: %1 bits").arg(progress.sourceFormat.bitsPerSample));
            textEdit_->append(QString("格式: %1").arg(QString::fromStdString(progress.sourceFormat.format)));
            textEdit_->append(QString("文件大小: %1 bytes").arg(progress.totalBytes));
            textEdit_->append("\n=== 转换进度 ===");
        }
        
        // 更新进度信息
        textEdit_->append(QString("转换进度: %1%").arg(progressValue));
    });

    // 开始转换
    for (const auto& inputFile : inputFiles_) {
        // 获取文件所在目录和工作目录
        std::filesystem::path inputPath(inputFile);
        std::string parentDir = inputPath.parent_path().string();
        std::string fileNameWithoutExt = inputPath.stem().string();
        std::string workDir = (std::filesystem::path(parentDir) / fileNameWithoutExt).string();
        
        // 生成输出文件名
        std::string outputFile = (std::filesystem::path(workDir) / 
                                (fileNameWithoutExt + ".ogg")).string();
        
        textEdit_->append(QString("\n开始转换: %1").arg(QString::fromStdString(inputFile)));
        textEdit_->append(QString("输出文件: %1").arg(QString::fromStdString(outputFile)));
        
        std::cout << "[DEBUG] Starting conversion for file: " << inputFile << std::endl;
        std::cout << "[DEBUG] Output will be saved to: " << outputFile << std::endl;
        
        if (audioConverter_->startConversion(inputFile, outputFile)) {
            textEdit_->append("转换线程已启动，等待完成...");
            std::cout << "[DEBUG] Conversion thread started successfully" << std::endl;
            
            // 等待转换完成
            while (audioConverter_->getCurrentProgress().progress < 1.0) {
                QApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            textEdit_->append(QString("转换完成: %1").arg(QString::fromStdString(outputFile)));
            std::cout << "[DEBUG] Conversion completed successfully" << std::endl;
        } else {
            std::string error = audioConverter_->getLastError();
            std::cerr << "[ERROR] Conversion failed: " << error << std::endl;
            textEdit_->append(QString("转换失败: %1").arg(QString::fromStdString(error)));
        }
    }

    // 重置进度条
    progressBar_->setValue(0);
    textEdit_->append("\n=== 转换过程结束 ===");
    std::cout << "[DEBUG] Conversion process completed" << std::endl;
    QMessageBox::information(this, "完成", "所有文件转换完成");
}

void AudioToTextWindow::setApiKey()
{
    // TODO: 实现API密钥设置逻辑
    QMessageBox::information(this, "提示", "API密钥设置功能待实现");
}

} // namespace ui
} // namespace perfx 