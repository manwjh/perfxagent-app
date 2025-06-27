#include "ui/realtime_audio_to_text_window.h"
#include "ui/config_manager.h"
#include "ui/global_state.h"
#include "logic/realtime_transcription_controller.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QTime>
#include <QTextStream>
#include <QFileInfo>
#include <QStyle>
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QComboBox>
#include <QFrame>
#include <QTimer>
#include <QTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QLabel>
#include <QCloseEvent>
#include <iostream>
#include <QRegularExpression>
#include <QMap>
#include <QIcon>
#include <QPixmap>
#include <QDebug>

namespace perfx {
namespace ui {

// =================================================================================
// 波形显示组件
// =================================================================================
class WaveformWidget : public QFrame {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr) : QFrame(parent), gain_(2.0f) {
        setMinimumHeight(100);
        setFrameStyle(QFrame::NoFrame);
        setStyleSheet("background-color: transparent;");
    }

    void updateWaveform(const QVector<float>& samples) {
        waveformData_ = samples;
        update(); // Request a repaint
    }

    void setGain(float gain) {
        gain_ = gain;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        QFrame::paintEvent(event);
        
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        
        painter.fillRect(rect(), QColor(30, 30, 30));

        if (waveformData_.isEmpty()) {
            painter.setPen(Qt::lightGray);
            painter.drawText(rect(), Qt::AlignCenter, "Select an audio source");
            return;
        }

        int width = this->width();
        int height = this->height();
        int centerY = height / 2;

        QPen pen(QColor(255, 0, 0, 150));
        pen.setWidth(2);
        painter.setPen(pen);

        // Draw waveform
        for (int i = 0; i < waveformData_.size(); ++i) {
            int x = (int)((float)i / waveformData_.size() * width);
            int barHeight = (int)(waveformData_[i] * centerY * 0.8 * gain_);
            painter.drawLine(x, centerY - barHeight, x, centerY + barHeight);
        }
    }

private:
    QVector<float> waveformData_;
    float gain_;
};

// =================================================================================
// 主窗口实现
// =================================================================================
RealtimeAudioToTextWindow::RealtimeAudioToTextWindow(QWidget *parent)
    : QWidget(parent),
      textEdit_(nullptr),
      statusLabel_(nullptr),
      recordPauseButton_(nullptr),
      stopButton_(nullptr),
      timerLabel_(nullptr),
      waveformWidget_(nullptr),
      audioSourceComboBox_(nullptr),
      statusBar_(nullptr),
      waveformFrame_(nullptr),
      mainLayout_(nullptr),
      timer_(nullptr),
      isRecording_(false),
      isPaused_(false),
      recordingStartTime_(0)
{
    controller_ = std::make_unique<logic::RealtimeTranscriptionController>(this);
    
    // Timer for recording duration
    timer_ = new QTimer(this);

    setupUI();
    setupMenuBar();
    setupStatusBar();
    setupBottomControls();
    connectSignals();
    updateStatusBarSuccess("待机中 - 选择音频源开始录音");
    updateRecordingButtons();
    controller_->refreshAudioDevices();
}

RealtimeAudioToTextWindow::~RealtimeAudioToTextWindow() {
    std::cout << "[UI] RealtimeAudioToTextWindow destructor called" << std::endl;
    
    if (controller_) {
        std::cout << "[CTRL] Calling controller shutdown..." << std::endl;
        controller_->shutdown();
        std::cout << "[CTRL] Controller shutdown completed" << std::endl;
    }
    
    std::cout << "[UI] RealtimeAudioToTextWindow destroyed." << std::endl;
}

void RealtimeAudioToTextWindow::setupUI() {
    setWindowTitle("Real-time Transcription");
    resize(400, 700);
    setStyleSheet("QWidget { background-color: #1E1E1E; }");

    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(0);
    mainLayout_->setContentsMargins(10, 10, 10, 10);

    // 伪菜单栏
    QMenuBar* menuBar = new QMenuBar(this);
    QMenu* fileMenu = menuBar->addMenu("文件");
    QAction* clearAction = fileMenu->addAction("清空文本");
    QAction* downloadAction = fileMenu->addAction("保存转写");
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction("退出");
    connect(clearAction, &QAction::triggered, this, &RealtimeAudioToTextWindow::clearText);
    connect(downloadAction, &QAction::triggered, this, &RealtimeAudioToTextWindow::downloadText);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    mainLayout_->addWidget(menuBar);

    // Create text display area
    textEdit_ = new QTextEdit(this);
    textEdit_->setReadOnly(true);
    textEdit_->setPlaceholderText("...");
    textEdit_->setStyleSheet(
        "QTextEdit {"
        "   background-color: #1E1E1E;"
        "   color: #F0F0F0;"
        "   border: none;"
        "   font-size: 16px;"
        "}"
    );
    
    // 性能优化设置
    textEdit_->setLineWrapMode(QTextEdit::WidgetWidth);
    textEdit_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    textEdit_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 减少重绘频率，提高性能
    textEdit_->document()->setMaximumBlockCount(1000); // 限制最大行数
    textEdit_->setUpdatesEnabled(true);
    
    mainLayout_->addWidget(textEdit_, 1);

    // 创建状态栏并设置为透明背景
    statusBar_ = new QStatusBar(this);
    statusBar_->setStyleSheet("QStatusBar { background-color: transparent; border: none; }");
    statusLabel_ = new QLabel("就绪", this);
    statusLabel_->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    statusBar_->addWidget(statusLabel_);
    mainLayout_->addWidget(statusBar_);

    mainLayout_->setStretch(0, 0); // menuBar
    mainLayout_->setStretch(1, 5); // textEdit_
    mainLayout_->setStretch(2, 0); // status bar
}

void RealtimeAudioToTextWindow::setupMenuBar() {}
void RealtimeAudioToTextWindow::setupStatusBar() {}

void RealtimeAudioToTextWindow::setupBottomControls() {
    QFrame* bottomFrame = new QFrame(this);
    bottomFrame->setStyleSheet("QFrame { background-color: #2E2E2E; border-radius: 15px; }");
    
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomFrame);
    bottomLayout->setContentsMargins(20, 20, 20, 20);
    bottomLayout->setSpacing(15);

    // Timer Label
    timerLabel_ = new QLabel("00:00.00", this);
    timerLabel_->setAlignment(Qt::AlignCenter);
    timerLabel_->setStyleSheet("QLabel { color: #FFFFFF; font-size: 36px; font-weight: bold; }");
    bottomLayout->addWidget(timerLabel_);
    
    // Waveform display
    waveformFrame_ = new WaveformWidget(this);
    bottomLayout->addWidget(waveformFrame_);

    // Controls Layout (Dropdown and buttons)
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    
    // Audio Source Dropdown
    audioSourceComboBox_ = new QComboBox(this);
    audioSourceComboBox_->setPlaceholderText("Select Source");
    audioSourceComboBox_->setStyleSheet(
        "QComboBox { "
        "   color: #FFFFFF; "
        "   background-color: #3E3E3E; "
        "   border: 1px solid #555555; "
        "   border-radius: 5px; padding: 5px; "
        "} "
        "QComboBox::drop-down { border: none; } "
        "QComboBox QAbstractItemView { background-color: #3E3E3E; color: #FFFFFF; }"
    );
    controlsLayout->addWidget(audioSourceComboBox_);

    controlsLayout->addStretch();

    // Record/Pause Button
    recordPauseButton_ = new QPushButton(this);
    recordPauseButton_->setCheckable(true);
    recordPauseButton_->setFixedSize(70, 70);
    recordPauseButton_->setStyleSheet(
        "QPushButton {"
        "   border: 2px solid #FF4500;"
        "   border-radius: 35px;"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #555555, stop:1 #333333);"
        "}"
        "QPushButton:checked {"
        "   background-color: #FF4500;"
        "}"
        "QPushButton:hover {"
        "   border-color: #FF6347;"
        "}"
    );
    controlsLayout->addWidget(recordPauseButton_);
    
    controlsLayout->addStretch();
    
    // Stop Button
    stopButton_ = new QPushButton("Stop", this);
    stopButton_->setStyleSheet("QPushButton { color: #AAAAAA; background-color: transparent; border: none; font-size: 14px; }");
    controlsLayout->addWidget(stopButton_);

    // Back to Main Menu Button
    QPushButton* backButton = new QPushButton("返回", this);
    backButton->setStyleSheet("QPushButton { color: #AAAAAA; background-color: transparent; border: none; font-size: 14px; }");
    controlsLayout->addWidget(backButton);
    connect(backButton, &QPushButton::clicked, this, &RealtimeAudioToTextWindow::backToMainMenu);

    bottomLayout->addLayout(controlsLayout);
    mainLayout_->addWidget(bottomFrame);
}

void RealtimeAudioToTextWindow::connectSignals() {
    connect(recordPauseButton_, &QPushButton::clicked, this, &RealtimeAudioToTextWindow::toggleRecording);
    connect(stopButton_, &QPushButton::clicked, this, &RealtimeAudioToTextWindow::stopRecording);
    
    connect(audioSourceComboBox_, QOverload<int>::of(&QComboBox::activated), 
            this, &RealtimeAudioToTextWindow::onAudioSourceChanged);
            
    // Connect controller signals to UI slots
    connect(controller_.get(), &logic::RealtimeTranscriptionController::audioDeviceListUpdated,
            this, &RealtimeAudioToTextWindow::updateAudioDeviceList);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::deviceSelectionResult,
            this, &RealtimeAudioToTextWindow::onDeviceSelectionResult);

    connect(controller_.get(), &logic::RealtimeTranscriptionController::waveformUpdated,
            this, &RealtimeAudioToTextWindow::updateWaveform);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrTranscriptionUpdated,
            this, &RealtimeAudioToTextWindow::onAsrTranscriptionUpdated, Qt::QueuedConnection);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrError,
            this, &RealtimeAudioToTextWindow::onAsrError, Qt::QueuedConnection);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrConnectionStatusChanged,
            this, &RealtimeAudioToTextWindow::onAsrConnectionStatusChanged, Qt::QueuedConnection);

    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrUtterancesUpdated,
            this, &RealtimeAudioToTextWindow::onAsrUtterancesUpdated);

    connect(timer_, &QTimer::timeout, this, &RealtimeAudioToTextWindow::updateTimerDisplay);
}

void RealtimeAudioToTextWindow::toggleRecording() {
    try {
        if (!isRecording_) {
            // 检查ASR状态
            if (perfx::ui::asr_valid == 0) {
                QMessageBox::warning(this, "ASR未就绪", 
                    "ASR配置未验证或验证失败。");
                return;
            }
            
            // 检查麦克风状态
            if (perfx::ui::mic_valid == 0) {
                QMessageBox::warning(this, "麦克风未就绪", 
                    "请先选择有效的音频源。");
                return;
            }
            
            // 录音前确保ASR线程已启动
            if (controller_ && !controller_->isAsrThreadRunning()) {
                try {
                    controller_->startAsrThread();
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Exception starting ASR thread: " << e.what() << std::endl;
                    QMessageBox::critical(this, "ASR错误", 
                        QString("启动ASR线程失败: %1").arg(e.what()));
                    return;
                } catch (...) {
                    std::cerr << "[ERROR] Unknown exception starting ASR thread" << std::endl;
                    QMessageBox::critical(this, "ASR错误", "启动ASR线程时发生未知错误");
                    return;
                }
            }
            
            // Start recording
            bool recordingStarted = false;
            try {
                recordingStarted = controller_->startRecording();
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Exception starting recording: " << e.what() << std::endl;
                QMessageBox::critical(this, "录音错误", 
                    QString("启动录音失败: %1").arg(e.what()));
                return;
            } catch (...) {
                std::cerr << "[ERROR] Unknown exception starting recording" << std::endl;
                QMessageBox::critical(this, "录音错误", "启动录音时发生未知错误");
                return;
            }
            
            if (recordingStarted) {
                isRecording_ = true;
                isPaused_ = false;
                recordingStartTime_ = QDateTime::currentMSecsSinceEpoch();
                timer_->start(100); // Update timer every 100ms
                updateStatusBarSuccess("录音中...");
                
                // 启动实时ASR
                if (controller_) {
                    try {
                        controller_->enableRealtimeAsr(true);
                    } catch (const std::exception& e) {
                        std::cerr << "[ERROR] Exception enabling realtime ASR: " << e.what() << std::endl;
                        // 不阻止录音，但显示警告
                        QMessageBox::warning(this, "ASR警告", 
                            QString("实时ASR启动失败，录音将继续但无实时转写: %1").arg(e.what()));
                    } catch (...) {
                        std::cerr << "[ERROR] Unknown exception enabling realtime ASR" << std::endl;
                        QMessageBox::warning(this, "ASR警告", "实时ASR启动时发生未知错误，录音将继续但无实时转写");
                    }
                }
            } else {
                QMessageBox::critical(this, "录音错误", "启动录音失败");
                return;
            }
        } else {
            // Pause/Resume recording
            isPaused_ = !isPaused_;
            if (isPaused_) {
                try {
                    controller_->pauseRecording();
                    updateStatusBarWarning("录音已暂停");
                    
                    // 暂停时禁用实时ASR
                    if (controller_) {
                        try {
                            controller_->enableRealtimeAsr(false);
                        } catch (const std::exception& e) {
                            std::cerr << "[ERROR] Exception disabling realtime ASR: " << e.what() << std::endl;
                        } catch (...) {
                            std::cerr << "[ERROR] Unknown exception disabling realtime ASR" << std::endl;
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Exception pausing recording: " << e.what() << std::endl;
                    QMessageBox::warning(this, "录音警告", 
                        QString("暂停录音时发生错误: %1").arg(e.what()));
                } catch (...) {
                    std::cerr << "[ERROR] Unknown exception pausing recording" << std::endl;
                    QMessageBox::warning(this, "录音警告", "暂停录音时发生未知错误");
                }
            } else {
                try {
                    controller_->resumeRecording();
                    updateStatusBarSuccess("录音中...");
                    
                    // 恢复时重新启用实时ASR
                    if (controller_) {
                        try {
                            controller_->enableRealtimeAsr(true);
                        } catch (const std::exception& e) {
                            std::cerr << "[ERROR] Exception re-enabling realtime ASR: " << e.what() << std::endl;
                            QMessageBox::warning(this, "ASR警告", 
                                QString("重新启用实时ASR失败: %1").arg(e.what()));
                        } catch (...) {
                            std::cerr << "[ERROR] Unknown exception re-enabling realtime ASR" << std::endl;
                            QMessageBox::warning(this, "ASR警告", "重新启用实时ASR时发生未知错误");
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Exception resuming recording: " << e.what() << std::endl;
                    QMessageBox::warning(this, "录音警告", 
                        QString("恢复录音时发生错误: %1").arg(e.what()));
                } catch (...) {
                    std::cerr << "[ERROR] Unknown exception resuming recording" << std::endl;
                    QMessageBox::warning(this, "录音警告", "恢复录音时发生未知错误");
                }
            }
        }
        updateRecordingButtons();
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in toggleRecording: " << e.what() << std::endl;
        QMessageBox::critical(this, "系统错误", 
            QString("录音操作时发生系统错误: %1").arg(e.what()));
    } catch (...) {
        std::cerr << "[ERROR] Unknown exception in toggleRecording" << std::endl;
        QMessageBox::critical(this, "系统错误", "录音操作时发生未知系统错误");
    }
}

void RealtimeAudioToTextWindow::stopRecording() {
    try {
        std::cout << "[UI] stopRecording() called, isRecording_: " << isRecording_ << std::endl;
        if (!isRecording_ && !isPaused_) return;

        if (controller_) {
            try {
                controller_->stopRecording();
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Exception stopping recording: " << e.what() << std::endl;
                QMessageBox::warning(this, "录音警告", 
                    QString("停止录音时发生错误: %1").arg(e.what()));
            } catch (...) {
                std::cerr << "[ERROR] Unknown exception stopping recording" << std::endl;
                QMessageBox::warning(this, "录音警告", "停止录音时发生未知错误");
            }
            
            // 停止录音时禁用ASR
            try {
                controller_->enableRealtimeAsr(false);
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Exception disabling ASR: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[ERROR] Unknown exception disabling ASR" << std::endl;
            }
            
            // 停止录音时关闭ASR线程
            try {
                controller_->stopAsrThread();
            } catch (const std::exception& e) {
                std::cerr << "[ERROR] Exception stopping ASR thread: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[ERROR] Unknown exception stopping ASR thread" << std::endl;
            }
        }
        
        isRecording_ = false;
        isPaused_ = false;
        timer_->stop();
        updateStatusBarSuccess("待机中 - 选择音频源开始录音");
        updateRecordingButtons();
        
        // 清空UI和控制器状态
        clearTranscription();
        
        std::cout << "[AUDIO-THREAD] Recording stopped." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in stopRecording: " << e.what() << std::endl;
        QMessageBox::critical(this, "系统错误", 
            QString("停止录音时发生系统错误: %1").arg(e.what()));
    } catch (...) {
        std::cerr << "[ERROR] Unknown exception in stopRecording" << std::endl;
        QMessageBox::critical(this, "系统错误", "停止录音时发生未知系统错误");
    }
}

void RealtimeAudioToTextWindow::updateRecordingButtons() {
    bool isRecordingAndNotPaused = isRecording_ && !isPaused_;
    recordPauseButton_->setChecked(isRecordingAndNotPaused);
    stopButton_->setEnabled(isRecording_);
    audioSourceComboBox_->setEnabled(!isRecording_);

    if (isRecordingAndNotPaused) {
        // Recording state - show pause icon
        recordPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        recordPauseButton_->setToolTip("暂停录音");
    } else if (isRecording_ && isPaused_) {
        // Paused state - show play icon
        recordPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        recordPauseButton_->setToolTip("继续录音");
    } else {
        // Standby state - show red dot
        recordPauseButton_->setIcon(createRedDotIcon());
        recordPauseButton_->setToolTip("开始录音");
    }
    
    // Adjust icon size if needed
    recordPauseButton_->setIconSize(QSize(35, 35));
}

QIcon RealtimeAudioToTextWindow::createRedDotIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw red circle
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 0, 0));
    painter.drawEllipse(8, 8, 16, 16);
    
    return QIcon(pixmap);
}

void RealtimeAudioToTextWindow::clearText() {
    textEdit_->clear();
    cumulativeText_.clear();
    partialText_.clear();
    finalLines_.clear();
}

void RealtimeAudioToTextWindow::downloadText() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save Transcript", QDir::homePath(), "Text Files (*.txt);;All Files (*)");
    if (fileName.isEmpty()) {
        return;
    }
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Could not save file.");
        return;
    }
    QTextStream out(&file);
    out << textEdit_->toPlainText();
    file.close();
    updateStatusBarSuccess("转写文本已保存到 " + fileName);
}

void RealtimeAudioToTextWindow::onDeviceSelectionResult(bool success, const QString& message) {
    if (success) {
        updateStatusBarSuccess(message);
    } else {
        updateStatusBarError(message);
    }
    if (!success) {
        QMessageBox::warning(this, "设备错误", message);
    }
}

void RealtimeAudioToTextWindow::onAudioSourceChanged(int index) {
    if (index < 0) {
        perfx::ui::mic_valid = 0;
        return;
    }
    
    int deviceId = audioSourceComboBox_->itemData(index).toInt();
    if (deviceId >= 0) {
        controller_->selectAudioDevice(deviceId);
        perfx::ui::mic_valid = 1;
        qDebug() << "✅ 音源选择成功";
    } else {
        perfx::ui::mic_valid = 0;
        qDebug() << "❌ 音源选择失败";
    }
}

void RealtimeAudioToTextWindow::updateStatusBar(const QString& message) {
    if (statusLabel_) {
        statusLabel_->setText(message);
        // 默认使用白色（成功类信息）
        statusLabel_->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    }
}

void RealtimeAudioToTextWindow::updateStatusBarSuccess(const QString& message) {
    if (statusLabel_) {
        statusLabel_->setText(message);
        // 成功类信息显示为白字
        statusLabel_->setStyleSheet("QLabel { color: #FFFFFF; background: transparent; }");
    }
}

void RealtimeAudioToTextWindow::updateStatusBarWarning(const QString& message) {
    if (statusLabel_) {
        statusLabel_->setText(message);
        // 警告类信息显示为红字
        statusLabel_->setStyleSheet("QLabel { color: #FF0000; background: transparent; }");
    }
}

void RealtimeAudioToTextWindow::updateStatusBarError(const QString& message) {
    if (statusLabel_) {
        statusLabel_->setText(message);
        // 失败类信息显示为黄字
        statusLabel_->setStyleSheet("QLabel { color: #FFFF00; background: transparent; }");
    }
}

void RealtimeAudioToTextWindow::closeEvent(QCloseEvent *event) {
    std::cout << "[UI] RealtimeAudioToTextWindow closeEvent called" << std::endl;
    
    // 关闭时停止所有音频任务
    stopMicCollection();
    
    // 关闭ASR线程
    if (controller_) {
        controller_->stopAsrThread();
    }
    
    // 确保定时器被停止
    if (timer_) {
        timer_->stop();
        std::cout << "[UI] Recording timer stopped" << std::endl;
    }
    
    // 清理UI状态
    isRecording_ = false;
    isPaused_ = false;
    perfx::ui::mic_valid = 0;
    
    event->accept();
    
    std::cout << "[UI] RealtimeAudioToTextWindow closeEvent completed" << std::endl;
}

// ============================================================================
// 实时ASR槽函数实现 (新增)
// ============================================================================

// 新增：最终句存储
QStringList finalLines_;

void RealtimeAudioToTextWindow::onAsrTranscriptionUpdated(const QString& text, bool isFinal) {
    (void)isFinal;
    if (!textEdit_) return;
    if (text.isEmpty()) return;

    // 目前 text 是多行字符串，无法区分 definite，全部视为 definite=true
    // 不要清空finalLines_，而是累积新的文本
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (!line.isEmpty()) {
            // 检查是否已经存在相同的行，避免重复添加
            if (!finalLines_.contains(line)) {
                finalLines_ << line;
            }
        }
    }
    // 预览区暂不处理（因无结构化数据）
    QString displayText = finalLines_.join("\n");
    textEdit_->setPlainText(displayText);
    QTextCursor cursor = textEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit_->setTextCursor(cursor);
    textEdit_->ensureCursorVisible();
}

void RealtimeAudioToTextWindow::onAsrError(const QString& error) {
    try {
        std::cerr << "[ERROR] ASR Error received: " << error.toStdString() << std::endl;
        
        // 显示错误信息
        QMessageBox::warning(this, "ASR错误", error);
        updateStatusBarWarning("ASR: 连接错误");
        
        // 如果正在录音，尝试自动恢复ASR连接
        if (isRecording_ && !isPaused_ && controller_) {
            std::cout << "[INFO] Attempting to recover ASR connection..." << std::endl;
            
            // 延迟一秒后尝试重新连接，避免立即重连
            QTimer::singleShot(1000, [this]() {
                try {
                    if (controller_) {
                        // 重置ASR状态
                        controller_->resetAsrState();
                        
                        // 重新启动ASR（startAsrThread返回void，不能用作条件判断）
                        controller_->startAsrThread();
                        
                        // 检查ASR线程是否成功启动
                        if (controller_->isAsrThreadRunning()) {
                            std::cout << "[INFO] ASR recovery successful" << std::endl;
                            updateStatusBarSuccess("ASR: 连接已恢复");
                        } else {
                            std::cout << "[WARNING] ASR recovery failed" << std::endl;
                            updateStatusBarWarning("ASR: 自动恢复失败");
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[ERROR] Exception during ASR recovery: " << e.what() << std::endl;
                    updateStatusBarError("ASR: 恢复时发生异常");
                } catch (...) {
                    std::cerr << "[ERROR] Unknown exception during ASR recovery" << std::endl;
                    updateStatusBarError("ASR: 恢复时发生未知异常");
                }
            });
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] Exception in onAsrError: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "[ERROR] Unknown exception in onAsrError" << std::endl;
    }
}

void RealtimeAudioToTextWindow::onAsrConnectionStatusChanged(bool connected) {
    if (connected) {
        updateStatusBarSuccess("ASR: 已连接");
    } else {
        updateStatusBarWarning("ASR: 连接断开");
    }
}

void RealtimeAudioToTextWindow::onAsrUtterancesUpdated(const QList<QVariantMap>& utterances) {
    // 不要每次都清空lines，而是累积新的内容
    QStringList newLines;
    QString previewLine;

    // 合并短句的最小长度
    const int minLineLen = 6;

    for (const auto& utter : utterances) {
        if (utter["definite"].toBool()) {
            QString txt = utter["text"].toString();
            if (txt.length() < minLineLen && !newLines.isEmpty()) {
                // 合并到上一行
                newLines.last() += txt;
            } else {
                newLines << txt;
            }
        } else {
            // 预览区
            QVariantList words = utter["words"].toList();
            QStringList wordList;
            for (const QVariant& w : words) {
                wordList << w.toMap()["text"].toString();
            }
            previewLine = wordList.join("");
        }
    }

    // 将新的最终行添加到finalLines_中，避免重复
    for (const QString& line : newLines) {
        if (!line.isEmpty() && !finalLines_.contains(line)) {
            finalLines_ << line;
        }
    }

    // 构建显示文本
    QString displayText = finalLines_.join("<br>");
    if (!previewLine.isEmpty()) {
        displayText += "<span style='color:gray;font-style:italic'>" + previewLine + "</span>";
    }
    textEdit_->setHtml(displayText);

    // 保持光标在末尾
    QTextCursor cursor = textEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    textEdit_->setTextCursor(cursor);
    textEdit_->ensureCursorVisible();
}

void RealtimeAudioToTextWindow::updateAudioDeviceList(const QStringList& names, const QList<int>& ids) {
    qDebug() << "[AudioDeviceList] names:" << names << "ids:" << ids;
    audioSourceComboBox_->clear();
    for (int i = 0; i < names.size(); ++i) {
        audioSourceComboBox_->addItem(names[i], ids[i]);
    }
}

void RealtimeAudioToTextWindow::updateTimerDisplay() {
    if (isRecording_ && !isPaused_) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - recordingStartTime_;
        QTime displayTime(0, 0, 0);
        displayTime = displayTime.addMSecs(elapsed);
        timerLabel_->setText(displayTime.toString("mm:ss.zzz").left(9));
    }
}

void RealtimeAudioToTextWindow::updateWaveform(const QVector<float>& samples) {
    if (auto* waveformWidget = qobject_cast<WaveformWidget*>(waveformFrame_)) {
        waveformWidget->updateWaveform(samples);
    }
}

void RealtimeAudioToTextWindow::clearTranscription() {
    textEdit_->clear();
    cumulativeText_.clear();
    partialText_.clear();
    finalLines_.clear();
}

void RealtimeAudioToTextWindow::saveRecording() {
    // This method can be implemented if needed. For now, it's just a placeholder.
}

void RealtimeAudioToTextWindow::startMicCollection() {
    // 初始化音频设备
    if (controller_) {
        controller_->refreshAudioDevices();
        perfx::ui::mic_valid = 1;  // 假设初始化成功
        qDebug() << "🎤 麦克风采集已开启";
    }
}

void RealtimeAudioToTextWindow::stopMicCollection() {
    std::cout << "[AUDIO-THREAD] stopMicCollection() called" << std::endl;
    
    // 停止录音和音频采集
    if (isRecording_) {
        std::cout << "[AUDIO-THREAD] Stopping active recording..." << std::endl;
        stopRecording();
    }
    
    // 停止定时器
    if (timer_) {
        timer_->stop();
        std::cout << "[UI] Recording timer stopped" << std::endl;
    }
    
    // 清理控制器
    if (controller_) {
        std::cout << "[CTRL] Shutting down controller..." << std::endl;
        controller_->shutdown();
        std::cout << "[CTRL] Controller shutdown completed" << std::endl;
    }
    
    // 重置状态
    isRecording_ = false;
    isPaused_ = false;
    perfx::ui::mic_valid = 0;
    
    // 清空音频源选择
    if (audioSourceComboBox_) {
        audioSourceComboBox_->clear();
        audioSourceComboBox_->setCurrentIndex(-1);
    }
    
    // 清空波形显示
    if (auto* waveformWidget = qobject_cast<WaveformWidget*>(waveformFrame_)) {
        waveformWidget->updateWaveform(QVector<float>());
    }
    
    qDebug() << "🎤 麦克风采集已关闭";
    std::cout << "[AUDIO-THREAD] stopMicCollection() completed" << std::endl;
}

void RealtimeAudioToTextWindow::backToMainMenu() {
    std::cout << "[UI] backToMainMenu() called" << std::endl;
    
    // 退出时关闭麦克风采集
    stopMicCollection();
    
    // 关闭ASR线程
    if (controller_) {
        controller_->stopAsrThread();
    }
    
    // 清空转写文本
    clearTranscription();
    
    // 重置UI状态
    updateStatusBarSuccess("已返回主菜单");
    updateRecordingButtons();
    
    std::cout << "[UI] backToMainMenu() completed, emitting signal" << std::endl;
    emit backToMainMenuRequested();
}

} // namespace ui
} // namespace perfx

#include "realtime_audio_to_text_window.moc" 
