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
#include <QTextCursor>
#include <QPushButton>
#include <QMenu>
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
// 增强的ASR回调类，用于处理ASR结果并更新UI
// =================================================================================
class EnhancedAsrCallback : public QObject, public Asr::AsrCallback {
    Q_OBJECT
public:
    EnhancedAsrCallback(audio::AudioManager* am, QTextEdit* te) 
        : audioManager_(am), textEdit_(te), currentText_(""), m_intermediateLine("") {
        std::cout << "[DEBUG] EnhancedAsrCallback构造函数" << std::endl;
        std::cout << "[DEBUG] audioManager_指针: " << (audioManager_ ? "有效" : "nullptr") << std::endl;
        std::cout << "[DEBUG] textEdit_指针: " << (textEdit_ ? "有效" : "nullptr") << std::endl;
    }

    void onOpen(Asr::AsrClient* client) override { (void)client; }

    void onClose(Asr::AsrClient* client) override {
        (void)client;
        emit finished();
    }

    void onMessage(Asr::AsrClient* client, const std::string& message) override {
        try {
            (void)client;
            std::cout << "[DEBUG] ASR回调收到消息，长度: " << message.length() << std::endl;
            std::cout << "[DEBUG] 消息内容: " << message << std::endl;
            
            QJsonDocument doc = QJsonDocument::fromJson(QString::fromUtf8(message.c_str()).toUtf8());
            if (!doc.isObject()) {
                std::cout << "[DEBUG] JSON解析失败，不是对象" << std::endl;
                return;
            }
            
            QJsonObject obj = doc.object();
            //std::cout << "[DEBUG] JSON对象键: " << obj.keys().join(", ").toStdString() << std::endl;
            
            if (!obj.contains("result")) {
                std::cout << "[DEBUG] JSON对象不包含result字段" << std::endl;
                return;
            }

            QJsonObject resultObj = obj["result"].toObject();
            //std::cout << "[DEBUG] result对象键: " << resultObj.keys().join(", ").toStdString() << std::endl;
            
            if (resultObj.contains("utterances") && resultObj["utterances"].isArray()) {
                QStringList all_utterances;
                for (const auto& utteranceVal : resultObj["utterances"].toArray()) {
                    QString text = utteranceVal.toObject()["text"].toString();
                    if (!text.isEmpty()) all_utterances.append(text);
                }
                currentText_ = all_utterances.join('\n');
                m_intermediateLine.clear();
                std::cout << "[DEBUG] 提取到utterances文本: " << currentText_.toStdString() << std::endl;
            } else if (resultObj.contains("text")) {
                QString text = resultObj["text"].toString();
                bool isFinal = resultObj.contains("is_final") ? resultObj.value("is_final").toBool() : false;
                std::cout << "[DEBUG] 提取到text: " << text.toStdString() << ", isFinal: " << isFinal << std::endl;
                if (isFinal) {
                    currentText_.append(text + "\n");
                    m_intermediateLine.clear();
                } else {
                    m_intermediateLine = "正在识别: " + text;
                }
            }
            
            std::cout << "[DEBUG] 当前文本: " << currentText_.toStdString() << std::endl;
            std::cout << "[DEBUG] 中间文本: " << m_intermediateLine.toStdString() << std::endl;
            std::cout << "[DEBUG] textEdit_指针: " << (textEdit_ ? "有效" : "nullptr") << std::endl;
            
            if (textEdit_) {
                QMetaObject::invokeMethod(this, [this]{
                    try {
                        QString displayText = currentText_ + m_intermediateLine;
                        std::cout << "[DEBUG] 设置UI文本: " << displayText.toStdString() << std::endl;
                        textEdit_->setPlainText(displayText);
                        textEdit_->moveCursor(QTextCursor::End);
                        std::cout << "[DEBUG] UI文本设置完成" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] UI更新异常: " << e.what() << std::endl;
                    }
                }, Qt::QueuedConnection);
            } else {
                std::cout << "[DEBUG] textEdit_为空，跳过UI更新" << std::endl;
            }
            
            std::cout << "[DEBUG] ASR回调处理完成" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] ASR回调异常: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[ERROR] ASR回调未知异常" << std::endl;
        }
    }

    void onError(Asr::AsrClient* client, const std::string& error) override {
        (void)client;
        if (textEdit_) {
             QMetaObject::invokeMethod(this, "appendError", Qt::QueuedConnection, 
                                      Q_ARG(QString, "ASR Error: " + QString::fromUtf8(error.c_str())));
        }
    }

public slots:
    void clearText() {
        if (!textEdit_) return;
        currentText_.clear();
        m_intermediateLine.clear();
        textEdit_->clear();
        textEdit_->moveCursor(QTextCursor::End);
    }

    void updateTextEdit(const QString& text, int sentenceIndex) {
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
    
    // 添加错误信息（不影响流式文本）
    void appendError(const QString& errorMsg) {
        if (!textEdit_) return;
        
        // 在流式文本下方添加错误信息
        textEdit_->append("\n" + errorMsg);
        textEdit_->moveCursor(QTextCursor::End);
    }

private:
    audio::AudioManager* audioManager_;
    QTextEdit* textEdit_;
    QMap<int, QString> sentences_;
    QString currentText_;      // 当前累积的最终文本
    QString m_intermediateLine; // 当前的中间识别结果

Q_SIGNALS:
    void finished();
};

AudioToTextWindow::AudioToTextWindow(QWidget *parent)
    : QMainWindow(parent),
      textEdit_(nullptr),
      statusBar_(nullptr),
      statusLabel_(nullptr),
      mediaPlayer_(nullptr),
      audioOutput_(nullptr),
      playButton_(nullptr),
      pauseButton_(nullptr),
      stopButton_(nullptr),
      playbackSlider_(nullptr),
      timeLabel_(nullptr),
      playbackLayout_(nullptr),
      hasWorkFiles_(false)
{
    std::cout << "[DEBUG] Initializing AudioToTextWindow..." << std::endl;
    
    fileImporter_ = std::make_unique<audio::FileImporter>();
    audioConverter_ = std::make_unique<audio::AudioConverter>();
    asrManager_ = std::make_unique<Asr::AsrManager>();
    
    if (!audioConverter_) {
        std::cerr << "[ERROR] Failed to create AudioConverter" << std::endl;
        QMessageBox::critical(this, "错误", "音频转换器初始化失败");
        return;
    }
    
    std::cout << "[DEBUG] AudioConverter created successfully" << std::endl;
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    // 在UI创建完成后，创建回调并设置给ASR管理器
    auto& audioManager = audio::AudioManager::getInstance();
    asrCallback_ = std::make_unique<EnhancedAsrCallback>(&audioManager, textEdit_);
    asrManager_->setCallback(asrCallback_.get());
    
    // 连接ASR完成信号
    connect(asrCallback_.get(), &EnhancedAsrCallback::finished, this, &AudioToTextWindow::onAsrFinished);
    
    updatePlaybackControls();
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

    // 添加音频播放器控件
    setupAudioPlayerControls();
    mainLayout->addLayout(playbackLayout_);

    // 添加导入和转录按钮区域
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* importButton = new QPushButton("导入文件", this);
    QPushButton* asrButton = new QPushButton("ASR转录", this);
    buttonLayout->addWidget(importButton);
    buttonLayout->addWidget(asrButton);
    mainLayout->addLayout(buttonLayout);

    // 连接信号和槽
    connect(importButton, &QPushButton::clicked, this, &AudioToTextWindow::importFile);
    connect(asrButton, &QPushButton::clicked, this, &AudioToTextWindow::asrTranscribe);

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

void AudioToTextWindow::setupStatusBar()
{
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusLabel_ = new QLabel("准备就绪", this);
    statusBar_->addWidget(statusLabel_);
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
            textEdit_->append(QString("转换完成: %1").arg(QString::fromUtf8(outputFile.c_str())));
            std::cout << "[DEBUG] Conversion completed successfully" << std::endl;
        } else {
            std::string error = audioConverter_->getLastError();
            std::cerr << "[ERROR] Conversion failed: " << error << std::endl;
            textEdit_->append(QString("转换失败: %1").arg(QString::fromUtf8(error.c_str())));
        }
    }

    // 重置进度条
    // progressBar_->setValue(0);
    updateStatusBar("=== 转换过程结束 ===");
    std::cout << "[DEBUG] Conversion process completed" << std::endl;
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
    auto& audioManager = audio::AudioManager::getInstance();
    const auto& workFile = *currentWorkFile_;
    std::filesystem::path workFilePath(workFile);
    std::string workDir = workFilePath.parent_path().string();
    
    std::string lrcFilePath = (std::filesystem::path(workDir) / (workFilePath.stem().string() + "_lyrics.lrc")).string();
    std::string jsonFilePath = (std::filesystem::path(workDir) / (workFilePath.stem().string() + "_lyrics.json")).string();
        
    audioManager.saveLyricsToFile(lrcFilePath, "lrc");
    audioManager.saveLyricsToFile(jsonFilePath, "json");

    // 在文本框中显示保存路径
    textEdit_->append("\n--- 转录完成 ---");
    textEdit_->append("LRC 歌词文件已保存到: " + QString::fromStdString(lrcFilePath));
    textEdit_->append("JSON 歌词文件已保存到: " + QString::fromStdString(jsonFilePath));
        
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
    std::cout << "[DEBUG] 开始处理文件: " << workFile << std::endl;
    updateStatusBar(QString("正在转录: %1...").arg(QString::fromStdString(workFile)));
    
    // Clear previous results and start recognition
    auto& audioManager = audio::AudioManager::getInstance();
    audioManager.clearLyrics();
    
    // 确保回调被正确设置
    if (asrCallback_) {
        std::cout << "[DEBUG] ASR回调已设置，开始识别" << std::endl;
        asrManager_->recognizeAudioFileAsync(workFile);
    } else {
        std::cerr << "[ERROR] ASR回调未设置" << std::endl;
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
    if (!asrManager_) {
        QMessageBox::critical(this, "错误", "ASR管理器未初始化");
        return;
    }
    
    std::cout << "[DEBUG] 开始ASR转录" << std::endl;
    std::cout << "[DEBUG] textEdit_指针: " << (textEdit_ ? "有效" : "nullptr") << std::endl;
    std::cout << "[DEBUG] asrCallback_指针: " << (asrCallback_ ? "有效" : "nullptr") << std::endl;
    
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
    if (statusBar_) {
        statusLabel_->setText(message);
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

} // namespace ui
} // namespace perfx 

#include "audio_to_text_window.moc" 