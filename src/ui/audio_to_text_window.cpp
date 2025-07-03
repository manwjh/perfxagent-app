#include "ui/audio_to_text_window.h"
#include "ui/config_manager.h"
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
#include <QTextCursor>
#include <QPushButton>
#include <QMenu>
#include <QCloseEvent>
#include <thread>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStatusBar>
#include <QToolButton>
#include <QSlider>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QStyle>
#include <QLabel>

namespace perfx {
namespace ui {

// =================================================================================
// EnhancedAsrCallback 实现
// =================================================================================

EnhancedAsrCallback::EnhancedAsrCallback(QTextEdit* te)
    : textEdit_(te), currentText_(""), m_intermediateLine("") {}

void EnhancedAsrCallback::onOpen(Asr::AsrClient* client) { (void)client; }

void EnhancedAsrCallback::onClose(Asr::AsrClient* client) {
    (void)client;
    emit finished();
}

void EnhancedAsrCallback::onMessage(Asr::AsrClient* client, const std::string& message) {
    try {
        (void)client;
        std::cout << "[UI] ASR回调收到消息，长度: " << message.length() << std::endl;
        std::cout << "[UI] 消息内容: " << message << std::endl;
        
        QJsonDocument doc = QJsonDocument::fromJson(QString::fromUtf8(message.c_str()).toUtf8());
        if (!doc.isObject()) {
            std::cout << "[UI] JSON解析失败，不是对象" << std::endl;
            return;
        }
        
        QJsonObject obj = doc.object();
        
        if (!obj.contains("result")) {
            std::cout << "[UI] JSON对象不包含result字段" << std::endl;
            return;
        }

        QJsonObject resultObj = obj["result"].toObject();
        
        if (resultObj.contains("utterances") && resultObj["utterances"].isArray()) {
            QStringList all_utterances;
            for (const auto& utteranceVal : resultObj["utterances"].toArray()) {
                QString text = utteranceVal.toObject()["text"].toString();
                if (!text.isEmpty()) all_utterances.append(text);
            }
            currentText_ = all_utterances.join('\n');
            m_intermediateLine.clear();
            std::cout << "[UI] 提取到utterances文本: " << currentText_.toStdString() << std::endl;
        } else if (resultObj.contains("text")) {
            QString text = resultObj["text"].toString();
            
            // 检查是否为最终结果（通过definite字段或协议标志判断）
            bool isFinal = false;
            if (resultObj.contains("result") && resultObj["result"].toObject().contains("utterances")) {
                QJsonArray utterances = resultObj["result"].toObject()["utterances"].toArray();
                for (const QJsonValue& utterance : utterances) {
                    if (utterance.toObject().contains("definite") && utterance.toObject()["definite"].toBool()) {
                        isFinal = true;
                        break;
                    }
                }
            }
            
            std::cout << "[UI] 提取到text: " << text.toStdString() << ", isFinal: " << isFinal << std::endl;
            if (isFinal) {
                currentText_.append(text + "\n");
                m_intermediateLine.clear();
            } else {
                m_intermediateLine = "正在识别: " + text;
            }
        }
        
        std::cout << "[UI] 当前文本: " << currentText_.toStdString() << std::endl;
        std::cout << "[UI] 中间文本: " << m_intermediateLine.toStdString() << std::endl;
        std::cout << "[UI] textEdit_指针: " << (textEdit_ ? "有效" : "nullptr") << std::endl;
        
        if (textEdit_) {
            QMetaObject::invokeMethod(this, [this]{
                try {
                    QString displayText = currentText_ + m_intermediateLine;
                    std::cout << "[UI] 设置UI文本: " << displayText.toStdString() << std::endl;
                    textEdit_->setPlainText(displayText);
                    textEdit_->moveCursor(QTextCursor::End);
                    std::cout << "[UI] UI文本设置完成" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "[UI][ERROR] UI更新异常: " << e.what() << std::endl;
                }
            }, Qt::QueuedConnection);
        } else {
            std::cout << "[UI] textEdit_为空，跳过UI更新" << std::endl;
        }
        
        std::cout << "[UI] ASR回调处理完成" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[UI][ERROR] ASR回调异常: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[UI][ERROR] ASR回调未知异常" << std::endl;
    }
}

void EnhancedAsrCallback::onError(Asr::AsrClient* client, const std::string& error) {
    (void)client;
    if (textEdit_) {
         QMetaObject::invokeMethod(this, "appendError", Qt::QueuedConnection, 
                                  Q_ARG(QString, "ASR Error: " + QString::fromUtf8(error.c_str())));
    }
}

void EnhancedAsrCallback::clearText() {
    if (!textEdit_) return;
    currentText_.clear();
    m_intermediateLine.clear();
    textEdit_->clear();
    textEdit_->moveCursor(QTextCursor::End);
}

void EnhancedAsrCallback::updateTextEdit(const QString& text, int sentenceIndex) {
    if (!textEdit_) return;

    // Store the latest version of the sentence
    sentences_[sentenceIndex] = text;
    
    // Rebuild the full text from the map, which keeps sentences ordered by index
    QStringList allLines;
    for(const QString &line : sentences_.values()) {
        allLines.append(line);
    }

    textEdit_->setPlainText(allLines.join('\n'));
    textEdit_->moveCursor(QTextCursor::End); // Keep cursor at the end
}

void EnhancedAsrCallback::appendError(const QString& errorMsg) {
    if (!textEdit_) return;
    
    // 在流式文本下方添加错误信息
    textEdit_->append("\n" + errorMsg);
    textEdit_->moveCursor(QTextCursor::End);
}

// =================================================================================
// AudioToTextWindow 实现
// =================================================================================
// AudioToTextWindow 构造函数
// =================================================================================
/**
 * @brief AudioToTextWindow 构造函数
 * 
 * 初始化音频转文本窗口，包括：
 * 1. 初始化UI组件（文本编辑器、播放控件、按钮等）
 * 2. 创建音频处理组件（文件导入器、音频转换器）
 * 3. 获取ASR管理器单例实例
 * 4. 设置ASR回调函数
 * 5. 连接信号和槽
 * 6. 重置窗口状态
 * 
 * @param parent 父窗口指针，用于Qt对象树管理
 * 
 * @note 该构造函数采用异常安全的设计，确保在初始化失败时能够正确清理资源
 * @note ASR管理器使用单例模式，避免重复创建实例
 * @note 所有UI组件在构造函数中创建，确保窗口完全可用
 */
AudioToTextWindow::AudioToTextWindow(QWidget *parent)
    : QWidget(parent),
      textEdit_(nullptr),           // 文本编辑区域，显示识别结果
      statusBar_(nullptr),          // 状态栏（已废弃，保留兼容性）
      statusLabel_(nullptr),        // 状态标签（已废弃，保留兼容性）
      mediaPlayer_(nullptr),        // 媒体播放器，用于音频播放
      audioOutput_(nullptr),        // 音频输出设备
      playButton_(nullptr),         // 播放按钮
      pauseButton_(nullptr),        // 暂停按钮
      stopButton_(nullptr),         // 停止按钮
      playbackSlider_(nullptr),     // 播放进度滑块
      timeLabel_(nullptr),          // 时间显示标签
      playbackLayout_(nullptr),     // 播放控件布局
      hasWorkFiles_(false)          // 是否有可处理的工作文件
{
    std::cout << "[UI] Initializing AudioToTextWindow..." << std::endl;
    try {
        // 步骤1: 创建音频处理组件
        fileImporter_ = std::make_unique<audio::FileImporter>();    // 文件导入器
        audioConverter_ = std::make_unique<audio::AudioConverter>(); // 音频格式转换器
        
        // 步骤2: 获取ASR管理器单例实例（避免重复创建）
        asrManager_ = &Asr::AsrManager::instance();
        
        // 步骤3: 验证音频转换器创建是否成功
        if (!audioConverter_) {
            std::cerr << "[UI][ERROR] Failed to create AudioConverter" << std::endl;
            QMessageBox::critical(this, "错误", "音频转换器初始化失败");
            return;
        }
        
        std::cout << "[UI] AudioConverter created successfully" << std::endl;
        
        // 步骤4: 初始化用户界面
        setupUI();           // 设置主界面布局和控件
        setupMenuBar();      // 设置菜单栏（当前为空实现）
        setupStatusBar();    // 设置状态栏（当前为空实现）
        
        // 步骤5: 创建并设置ASR回调
        // 创建增强型ASR回调对象，传入文本编辑器用于显示识别结果
        asrCallback_ = std::make_unique<EnhancedAsrCallback>(textEdit_);
        asrManager_->setCallback(asrCallback_.get());
        
        // 步骤6: 连接ASR完成信号到对应的槽函数
        connect(asrCallback_.get(), &EnhancedAsrCallback::finished, 
                this, &AudioToTextWindow::onAsrFinished);
        
        // 步骤7: 更新播放控件状态
        updatePlaybackControls();
        
        std::cout << "[UI] AudioToTextWindow initialization completed" << std::endl;
        
        // 步骤8: 重置窗口状态，确保初始状态一致
        resetState();
        
    } catch (const std::exception& e) {
        // 异常处理：记录具体异常信息
        std::cerr << "[UI][FATAL] Exception in AudioToTextWindow constructor: " << e.what() << std::endl;
    } catch (...) {
        // 异常处理：记录未知异常
        std::cerr << "[UI][FATAL] Unknown exception in AudioToTextWindow constructor!" << std::endl;
    }
}

AudioToTextWindow::~AudioToTextWindow() {
    std::cout << "[UI] AudioToTextWindow destructor called (begin)" << std::endl;
    // 停止媒体播放器
    if (mediaPlayer_) {
        mediaPlayer_->stop();
    }
    // 停止 ASR 识别线程
    if (asrManager_) {
        std::cout << "[UI] Stopping ASR recognition..." << std::endl;
        asrManager_->stopRecognition();
        std::cout << "[UI] ASR recognition stopped" << std::endl;
    }
    // 断开 ASR 连接
    if (asrManager_) {
        std::cout << "[UI] Disconnecting ASR..." << std::endl;
        asrManager_->disconnect();
        std::cout << "[AUDIO_TO_TEXT_WINDOWS] ASR disconnected" << std::endl;
    }
    // 清理 ASR 回调
    if (asrCallback_) {
        std::cout << "[UI] Cleaning up ASR callback..." << std::endl;
        asrCallback_.reset();
        std::cout << "[UI] ASR callback cleaned up" << std::endl;
    }
    // 清理音频转换器
    if (audioConverter_) {
        std::cout << "[UI] Cleaning up audio converter..." << std::endl;
        audioConverter_->stopConversion();
        audioConverter_.reset();
        std::cout << "[UI] Audio converter cleaned up" << std::endl;
    }
    // 清理文件导入器
    if (fileImporter_) {
        std::cout << "[UI] Cleaning up file importer..." << std::endl;
        fileImporter_.reset();
        std::cout << "[UI] File importer cleaned up" << std::endl;
    }
    std::cout << "[UI] AudioToTextWindow destructor completed" << std::endl;
}

void AudioToTextWindow::setupUI()
{
    setWindowTitle("录音转文本");
    resize(800, 600);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 添加文本编辑区域
    textEdit_ = new QTextEdit(this);
    textEdit_->setReadOnly(true);
    mainLayout->addWidget(textEdit_);

    // 添加音频播放器控件
    setupAudioPlayerControls();
    mainLayout->addLayout(playbackLayout_);

    // 添加导入和转录按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* importButton = new QPushButton("导入文件", this);
    QPushButton* asrButton = new QPushButton("ASR转录", this);
    QPushButton* backButton = new QPushButton("返回", this);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(asrButton);
    buttonLayout->addWidget(backButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号和槽
    connect(importButton, &QPushButton::clicked, this, &AudioToTextWindow::importFile);
    connect(asrButton, &QPushButton::clicked, this, &AudioToTextWindow::asrTranscribe);
    connect(backButton, &QPushButton::clicked, this, &AudioToTextWindow::backToMainMenu);

    // 连接信号
    connectSignals();
}

void AudioToTextWindow::setupMenuBar()
{
    // 由于现在是QWidget，不再需要菜单栏
    // 如果需要菜单功能，可以创建QMenu并添加到按钮上
}

void AudioToTextWindow::setupStatusBar()
{
    // 由于现在是QWidget，不再需要状态栏
    // 状态信息可以显示在文本编辑器中或其他地方
}

void AudioToTextWindow::setupAudioPlayerControls()
{
    playbackLayout_ = new QHBoxLayout();

    playButton_ = new QToolButton(this);
    playButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(playButton_, &QToolButton::clicked, this, &AudioToTextWindow::playAudio);

    pauseButton_ = new QToolButton(this);
    pauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    pauseButton_->setVisible(false); // 默认隐藏
    connect(pauseButton_, &QToolButton::clicked, this, &AudioToTextWindow::pauseAudio);

    stopButton_ = new QToolButton(this);
    stopButton_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    connect(stopButton_, &QToolButton::clicked, this, &AudioToTextWindow::stopAudio);

    playbackSlider_ = new QSlider(Qt::Horizontal, this);
    connect(playbackSlider_, &QSlider::sliderMoved, this, &AudioToTextWindow::onPlaybackSliderMoved);

    timeLabel_ = new QLabel("00:00 / 00:00", this);

    playbackLayout_->addWidget(playButton_);
    playbackLayout_->addWidget(pauseButton_);
    playbackLayout_->addWidget(stopButton_);
    playbackLayout_->addWidget(playbackSlider_);
    playbackLayout_->addWidget(timeLabel_);

    mediaPlayer_ = new QMediaPlayer(this);
    audioOutput_ = new QAudioOutput(this);
    mediaPlayer_->setAudioOutput(audioOutput_);

    connect(mediaPlayer_, &QMediaPlayer::positionChanged, this, &AudioToTextWindow::onPlaybackPositionChanged);
    connect(mediaPlayer_, &QMediaPlayer::durationChanged, this, &AudioToTextWindow::onPlaybackDurationChanged);
    connect(mediaPlayer_, &QMediaPlayer::playbackStateChanged, this, &AudioToTextWindow::onMediaPlayerStateChanged);
    connect(mediaPlayer_, &QMediaPlayer::mediaStatusChanged, this, &AudioToTextWindow::onMediaStatusChanged);
    // connect(mediaPlayer_, qOverload<QMediaPlayer::Error, const QString&>(&QMediaPlayer::error), this, &AudioToTextWindow::onMediaPlayerError);
}

void AudioToTextWindow::connectSignals() {
    // 只保留与文件转换相关的信号（如 audioConverter_ 的信号）
    // 如果没有则可留空
}

void AudioToTextWindow::onOutputFileInfo(const QString& info) {
    textEdit_->append(info);
}

void AudioToTextWindow::onConversionProgress(int progress) {
    // 状态栏显示进度
    updateStatusBar(QString("转换进度: %1%").arg(progress));
}

void AudioToTextWindow::onConversionComplete(const QString& outputFile) {
    textEdit_->append("转换完成: " + outputFile);
    updateStatusBar("转换完成: " + outputFile);
}

void AudioToTextWindow::onError(const QString& errorMessage) {
    updateStatusBar("错误: " + errorMessage);
    QMessageBox::warning(this, "错误", errorMessage);
}

void AudioToTextWindow::importFile()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QStringList fileNames = QFileDialog::getOpenFileNames(this, "选择音频文件", defaultDir, "音频文件 (*.wav *.mp3 *.ogg *.flac *.m4a *.aac);;所有文件 (*.*)");

    if (fileNames.isEmpty()) {
        return;
    }

    inputFiles_.clear();
    workFiles_.clear();
    textEdit_->clear();
    updateStatusBar(QString("正在导入 %1 个文件...").arg(fileNames.length()));

    for (const QString& fileName : fileNames) {
        std::string filePath = fileName.toUtf8().toStdString();
        inputFiles_.push_back(filePath);
        
        std::filesystem::path path(filePath);
        std::string workDir = (path.parent_path() / path.stem()).string();
        
        try {
            std::filesystem::create_directories(workDir);
        } catch (const std::filesystem::filesystem_error& e) {
            updateStatusBar(QString("创建目录失败: %1").arg(e.what()));
            QMessageBox::critical(this, "文件系统错误", QString("无法创建工作目录: %1. 错误: %2").arg(QString::fromStdString(workDir), e.what()));
            return;
        }

        std::string workFile = (std::filesystem::path(workDir) / (path.stem().string() + ".wav")).string();
        workFiles_.push_back(workFile);
        
        convertToWavFile(filePath, workFile);
    }
    
    if (!workFiles_.empty()) {
        hasWorkFiles_ = true;
    }
    updatePlaybackControls();
    updateStatusBar(QString("✅ %1 个文件导入转换完成，可以开始ASR转录。").arg(inputFiles_.size()));
}

void AudioToTextWindow::convertAudio()
{
    if (inputFiles_.empty()) {
        QMessageBox::warning(this, "警告", "请先导入音频文件");
        return;
    }

    if (!audioConverter_) {
        std::cerr << "[UI] [ERROR] AudioConverter is not initialized" << std::endl;
        QMessageBox::critical(this, "错误", "音频转换器未初始化");
        return;
    }

    textEdit_->append("\n=== 开始转换过程 ===");
    std::cout << "[UI] [DEBUG] Starting conversion process..." << std::endl;

    // 设置进度回调
    audioConverter_->setProgressCallback([this](const audio::ConversionProgress& progress) {
        std::cout << "[UI] [DEBUG] Progress callback triggered: " << (progress.progress * 100) << "%" << std::endl;
        
        // 更新进度条
        int progressValue = static_cast<int>(progress.progress * 100);
        // progressBar_->setValue(progressValue);
        
        // 显示音频信息
        if (progress.progress == 0) {
            textEdit_->clear();
            textEdit_->append("\n=== 音频文件信息 ===");
            textEdit_->append(QString("采样率: %1 Hz").arg(progress.sourceFormat.sampleRate));
            textEdit_->append(QString("声道数: %1").arg(static_cast<int>(progress.sourceFormat.channels)));
            textEdit_->append(QString("位深度: %1 bits").arg(progress.sourceFormat.bitsPerSample));
            textEdit_->append(QString("格式: %1").arg(QString::fromStdString(progress.sourceFormat.format)));
            textEdit_->append(QString("文件大小: %1 bytes").arg(progress.totalBytes));
            textEdit_->append("\n=== 转换进度 ===");
        }
        
        // 更新进度信息
        updateStatusBar(QString("转换进度: %1%").arg(progressValue));
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
        
        textEdit_->append(QString("\n开始转换: %1").arg(QString::fromUtf8(inputFile.c_str())));
        textEdit_->append(QString("输出文件: %1").arg(QString::fromUtf8(outputFile.c_str())));
        
        std::cout << "[UI] [DEBUG] Starting conversion for file: " << inputFile << std::endl;
        std::cout << "[UI] [DEBUG] Output will be saved to: " << outputFile << std::endl;
        
        if (audioConverter_->startConversion(inputFile, outputFile)) {
            textEdit_->append("转换线程已启动，等待完成...");
            std::cout << "[UI] [DEBUG] Conversion thread started successfully" << std::endl;
            
            // 等待转换完成
            while (audioConverter_->getCurrentProgress().progress < 1.0) {
                QApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            textEdit_->append(QString("转换完成: %1").arg(QString::fromUtf8(outputFile.c_str())));
            std::cout << "[UI] [DEBUG] Conversion completed successfully" << std::endl;
        } else {
            std::string error = audioConverter_->getLastError();
            std::cerr << "[UI] [ERROR] Conversion failed: " << error << std::endl;
            textEdit_->append(QString("转换失败: %1").arg(QString::fromUtf8(error.c_str())));
        }
    }

    // 重置进度条
    // progressBar_->setValue(0);
    updateStatusBar("=== 转换过程结束 ===");
    std::cout << "[UI] [DEBUG] Conversion process completed" << std::endl;
    QMessageBox::information(this, "完成", "所有文件转换完成");
}

void AudioToTextWindow::setApiKey()
{
    // TODO: 实现API密钥设置逻辑
    QMessageBox::information(this, "提示", "API密钥设置功能待实现");
}

// 音频播放控制槽函数
void AudioToTextWindow::playAudio()
{
    if (workFiles_.empty()) {
        QMessageBox::warning(this, "提示", "没有可播放的文件。");
        return;
    }
    
    // 默认播放第一个工作文件
    const std::string& fileToPlay = workFiles_.front();

    if (!std::filesystem::exists(fileToPlay)) {
         QMessageBox::critical(this, "错误", QString("文件不存在: %1").arg(QString::fromUtf8(fileToPlay.c_str())));
        return;
    }

    // 检查当前媒体源是否与要播放的文件相同
    QUrl currentSource = mediaPlayer_->source();
    QUrl newSource = QUrl::fromLocalFile(QString::fromUtf8(fileToPlay.c_str()));
    
    // 如果媒体源不同，需要重新设置
    if (currentSource != newSource) {
        mediaPlayer_->setSource(newSource);
        
        // 等待媒体源加载完成后再播放
        connect(mediaPlayer_, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::LoadedMedia) {
                // 媒体加载完成，开始播放
                mediaPlayer_->play();
                // 断开这个临时连接，避免重复触发
                disconnect(mediaPlayer_, &QMediaPlayer::mediaStatusChanged, this, nullptr);
            } else if (status == QMediaPlayer::InvalidMedia) {
                // 媒体无效，显示错误
                updateStatusBar("错误: 无法加载媒体文件");
                disconnect(mediaPlayer_, &QMediaPlayer::mediaStatusChanged, this, nullptr);
            }
        });
    } else {
        // 媒体源相同，直接播放
        mediaPlayer_->play();
    }
}

void AudioToTextWindow::pauseAudio()
{
    mediaPlayer_->pause();
}

void AudioToTextWindow::stopAudio()
{
    mediaPlayer_->stop();
}

void AudioToTextWindow::onPlaybackPositionChanged(qint64 position)
{
    if (!playbackSlider_->isSliderDown()) {
        playbackSlider_->setValue(position);
    }
    qint64 duration = mediaPlayer_->duration();
    timeLabel_->setText(QString("%1 / %2").arg(formatTime(position), formatTime(duration)));
}

void AudioToTextWindow::onPlaybackDurationChanged(qint64 duration)
{
    playbackSlider_->setRange(0, duration);
    qint64 position = mediaPlayer_->position();
    timeLabel_->setText(QString("%1 / %2").arg(formatTime(position), formatTime(duration)));
}

void AudioToTextWindow::onPlaybackSliderMoved(int position)
{
    mediaPlayer_->setPosition(position);
}

void AudioToTextWindow::onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state)
{
    if (state == QMediaPlayer::PlayingState) {
        playButton_->setVisible(false);
        pauseButton_->setVisible(true);
        updateStatusBar("正在播放");
    } else if (state == QMediaPlayer::PausedState) {
        playButton_->setVisible(true);
        pauseButton_->setVisible(false);
        updateStatusBar("已暂停");
    } else if (state == QMediaPlayer::StoppedState) {
        playButton_->setVisible(true);
        pauseButton_->setVisible(false);
        updateStatusBar("已停止");
    }
}

void AudioToTextWindow::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    if (status == QMediaPlayer::InvalidMedia) {
        updateStatusBar("错误: 无法加载媒体文件");
    }
}

void AudioToTextWindow::onMediaPlayerError(QMediaPlayer::Error error, const QString& errorString)
{
    (void)error;
    updateStatusBar("播放器错误: " + errorString);
    QMessageBox::critical(this, "播放器错误", errorString);
    
    // 错误时重置按钮状态
    playButton_->setVisible(true);
    pauseButton_->setVisible(false);
}

// 歌词相关槽函数
void AudioToTextWindow::viewLyrics()
{
    // TODO: 实现歌词查看逻辑
    QMessageBox::information(this, "提示", "歌词查看功能待实现");
}

void AudioToTextWindow::exportLrcFile()
{
    // TODO: 实现LRC文件导出逻辑
    QMessageBox::information(this, "提示", "LRC文件导出功能待实现");
}

void AudioToTextWindow::exportJsonFile()
{
    // TODO: 实现JSON文件导出逻辑
    QMessageBox::information(this, "提示", "JSON文件导出功能待实现");
}

void AudioToTextWindow::clearLyrics()
{
    // TODO: 实现歌词清除逻辑
    QMessageBox::information(this, "提示", "歌词清除功能待实现");
}

void AudioToTextWindow::onAsrFinished()
{
    if (currentWorkFile_ == workFiles_.end()) return;

    // Save results for the completed file
    const auto& workFile = *currentWorkFile_;
    std::filesystem::path workFilePath(workFile);
    std::string workDir = workFilePath.parent_path().string();
    
    std::string lrcFilePath = (std::filesystem::path(workDir) / (workFilePath.stem().string() + "_lyrics.lrc")).string();
    std::string jsonFilePath = (std::filesystem::path(workDir) / (workFilePath.stem().string() + "_lyrics.json")).string();
        
    // Process the next file
    ++currentWorkFile_;
    startNextAsrTask();
}

void AudioToTextWindow::startNextAsrTask()
{
    if (currentWorkFile_ == workFiles_.end()) {
        updateStatusBar("所有文件转录完成。");
        QMessageBox::information(this, "完成", "所有文件转录完成");
        return;
    }

    const auto& workFile = *currentWorkFile_;
    std::cout << "[UI] 开始处理文件: " << workFile << std::endl;
    updateStatusBar(QString("正在转录: %1...").arg(QString::fromStdString(workFile)));
    
    // 确保回调被正确设置
    if (asrCallback_) {
        std::cout << "[UI] ASR回调已设置，开始识别" << std::endl;
        asrManager_->recognizeAudioFileAsync(workFile);
    } else {
        std::cerr << "[UI][ERROR] ASR回调未设置" << std::endl;
        QMessageBox::critical(this, "错误", "ASR回调未设置");
    }
}

// ASR转录槽函数
void AudioToTextWindow::asrTranscribe()
{
    if (!hasWorkFiles_ || workFiles_.empty()) {
        QMessageBox::warning(this, "警告", "请先导入并转换文件。");
        return;
    }
    
    // 检查ASR配置是否有效
    auto configManager = perfx::ui::ConfigManager::instance();
    if (!configManager->hasValidConfig()) {
        QMessageBox::warning(this, "ASR配置提示", 
            "ASR Cloud配置未设置或无效。\n\n"
            "请前往系统配置界面设置您的ASR Cloud账号信息。",
            QMessageBox::Ok);
        
        // 发送信号通知主窗口跳转到设置界面
        emit backToMainMenuRequested();
        return;
    }
    
    if (!asrManager_) {
        QMessageBox::critical(this, "错误", "ASR管理器未初始化");
        return;
    }
    
    // 转录前确保ASR线程已启动
    if (asrManager_ && !asrManager_->isAsrThreadRunning()) {
        asrManager_->startAsrThread();
    }
    
    std::cout << "[UI] 开始ASR转录" << std::endl;
    std::cout << "[UI] textEdit_指针: " << (textEdit_ ? "有效" : "nullptr") << std::endl;
    std::cout << "[UI] asrCallback_指针: " << (asrCallback_ ? "有效" : "nullptr") << std::endl;
    
    textEdit_->clear();
    textEdit_->append("=== ASR 转录开始 ===");
    
    // Start the first task, the rest will be chained by the finished signal
    currentWorkFile_ = workFiles_.begin();
    startNextAsrTask();
}

void AudioToTextWindow::convertToWavFile(const std::string& inputFile, const std::string& outputFile)
{
    if (!audioConverter_) {
        updateStatusBar("错误: 音频转换器未初始化");
        return;
    }

    textEdit_->append(QString("\n正在处理: %1").arg(QString::fromUtf8(inputFile.c_str())));
    
    audioConverter_->setProgressCallback([this](const audio::ConversionProgress& progress) {
        if (progress.progress == 0) {
            // Using QMetaObject to safely update UI from what might be a different thread.
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, "  === 源音频文件信息 ==="));
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, QString("  采样率: %1 Hz").arg(progress.sourceFormat.sampleRate)));
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, QString("  声道数: %1").arg(static_cast<int>(progress.sourceFormat.channels))));
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, QString("  位深度: %1 bits").arg(progress.sourceFormat.bitsPerSample)));
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, QString("  格式: %1").arg(QString::fromUtf8(progress.sourceFormat.format.c_str()))));
            QMetaObject::invokeMethod(textEdit_, "append", Qt::QueuedConnection, Q_ARG(QString, QString("  文件大小: %1 bytes").arg(progress.totalBytes)));
        }
    });
    
    if (audioConverter_->startConversion(inputFile, outputFile)) {
        updateStatusBar("转换开始...");
        while (audioConverter_->getCurrentProgress().progress < 1.0) {
            QApplication::processEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        textEdit_->append(QString("  ===转换的文件信息====="));
        textEdit_->append(QString("转换完成: %1").arg(QString::fromUtf8(outputFile.c_str())));
        updateStatusBar(QString("转换完成: %1").arg(QString::fromUtf8(outputFile.c_str())));
    } else {
        QString errorStr = QString::fromUtf8(audioConverter_->getLastError().c_str());
        textEdit_->append(QString("转换失败: %1").arg(errorStr));
        updateStatusBar(QString("转换失败: %1").arg(errorStr));
    }

    audioConverter_->setProgressCallback(nullptr);
}

void AudioToTextWindow::updateStatusBar(const QString& message)
{
    if (textEdit_) {
        textEdit_->append("[状态] " + message);
    }
}

void AudioToTextWindow::updatePlaybackControls() {
    bool enable = hasWorkFiles_ && !workFiles_.empty();
    playButton_->setEnabled(enable);
    stopButton_->setEnabled(enable);
    playbackSlider_->setEnabled(enable);
}

QString AudioToTextWindow::formatTime(qint64 milliseconds)
{
    int seconds = (milliseconds / 1000) % 60;
    int minutes = (milliseconds / (1000 * 60)) % 60;
    int hours = (milliseconds / (1000 * 60 * 60));
    if (hours > 0) {
        return QObject::tr("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
    }
    return QObject::tr("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

void AudioToTextWindow::backToMainMenu()
{
    std::cout << "[UI][DEBUG] AudioToTextWindow backToMainMenu called" << std::endl;
    
    // 停止媒体播放器
    if (mediaPlayer_) {
        mediaPlayer_->stop();
    }
    
    // 停止 ASR 识别线程
    if (asrManager_) {
        std::cout << "[UI][DEBUG] Stopping ASR recognition in backToMainMenu..." << std::endl;
        asrManager_->stopRecognition();
        std::cout << "[UI][DEBUG] ASR recognition stopped in backToMainMenu" << std::endl;
    }
    
    // 断开 ASR 连接
    if (asrManager_) {
        std::cout << "[UI][DEBUG] Disconnecting ASR in backToMainMenu..." << std::endl;
        asrManager_->disconnect();
        std::cout << "[UI][DEBUG] ASR disconnected in backToMainMenu" << std::endl;
    }
    
    // 关闭ASR线程
    if (asrManager_) {
        asrManager_->stopAsrThread();
    }
    
    // 返回主菜单前重置状态
    resetState();
    std::cout << "[UI][DEBUG] AudioToTextWindow backToMainMenu completed" << std::endl;
    emit backToMainMenuRequested();
}

void AudioToTextWindow::closeEvent(QCloseEvent *event)
{
    std::cout << "[UI][DEBUG] AudioToTextWindow closeEvent called (begin)" << std::endl;
    // 停止媒体播放器
    if (mediaPlayer_) {
        mediaPlayer_->stop();
    }
    // 停止 ASR 识别线程
    if (asrManager_) {
        std::cout << "[UI][DEBUG] Stopping ASR recognition in closeEvent..." << std::endl;
        asrManager_->stopRecognition();
        std::cout << "[UI][DEBUG] ASR recognition stopped in closeEvent" << std::endl;
    }
    // 断开 ASR 连接
    if (asrManager_) {
        std::cout << "[UI][DEBUG] Disconnecting ASR in closeEvent..." << std::endl;
        asrManager_->disconnect();
        std::cout << "[UI][DEBUG] ASR disconnected in closeEvent" << std::endl;
    }
    
    // 关闭ASR线程
    if (asrManager_) {
        asrManager_->stopAsrThread();
    }
    
    // 清理音频转换器
    if (audioConverter_) {
        std::cout << "[UI][DEBUG] Stopping audio conversion in closeEvent..." << std::endl;
        audioConverter_->stopConversion();
        std::cout << "[UI][DEBUG] Audio conversion stopped in closeEvent" << std::endl;
    }
    // 接受关闭事件
    event->accept();
    std::cout << "[UI][DEBUG] AudioToTextWindow closeEvent completed" << std::endl;
}

// =================================================================================
// 重置界面和数据状态的方法
// =================================================================================
/**
 * @brief 重置窗口状态到初始状态
 * 
 * 该方法用于将AudioToTextWindow的所有状态重置为初始值，包括：
 * 1. 清空文本编辑器内容
 * 2. 清空文件列表（输入文件和工作文件）
 * 3. 重置媒体播放器状态
 * 4. 重置播放控件状态
 * 5. 清空ASR回调中的文本内容
 * 
 * @note 该方法在构造函数中被调用，确保窗口初始状态一致
 * @note 在返回主菜单时也会被调用，确保下次进入时状态干净
 * @note 所有UI组件的状态都会被重置，但不会销毁组件本身
 */
void AudioToTextWindow::resetState()
{
    // 步骤1: 清空文本编辑框，移除所有显示内容
    if (textEdit_) {
        textEdit_->clear();
    }
    
    // 步骤2: 清空文件列表，释放内存
    inputFiles_.clear();     // 清空原始输入文件列表
    workFiles_.clear();      // 清空转换后的工作文件列表
    hasWorkFiles_ = false;   // 重置工作文件标志
    
    // 步骤3: 重置媒体播放器状态
    if (mediaPlayer_) {
        mediaPlayer_->stop();        // 停止当前播放
        mediaPlayer_->setSource(QUrl()); // 清空媒体源
    }
    
    // 步骤4: 重置播放控件状态
    if (playbackSlider_) {
        playbackSlider_->setValue(0);    // 重置进度条到起始位置
        playbackSlider_->setEnabled(false); // 禁用进度条
    }
    if (timeLabel_) {
        timeLabel_->setText("00:00 / 00:00"); // 重置时间显示
    }
    
    // 步骤5: 更新播放控件可用状态（基于hasWorkFiles_标志）
    updatePlaybackControls();
    
    // 步骤6: 清空ASR回调中的文本内容，确保下次识别时从空白开始
    if (asrCallback_) {
        asrCallback_->clearText();
    }
}

} // namespace ui
} // namespace perfx 

#include "audio_to_text_window.moc" 