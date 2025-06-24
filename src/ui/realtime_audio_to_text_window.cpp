#include "ui/realtime_audio_to_text_window.h"
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
    explicit WaveformWidget(QWidget* parent = nullptr) : QFrame(parent) {
        setMinimumHeight(100);
        setFrameStyle(QFrame::NoFrame);
        setStyleSheet("background-color: transparent;");
    }

    void updateWaveform(const QVector<float>& samples) {
        waveformData_ = samples;
        update(); // Request a repaint
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
            int barHeight = (int)(waveformData_[i] * centerY * 0.8);
            painter.drawLine(x, centerY - barHeight, x, centerY + barHeight);
        }
    }

private:
    QVector<float> waveformData_;
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
    updateStatusBar("待机中 - 选择音频源开始录音");
    updateRecordingButtons();
    controller_->refreshAudioDevices();
}

RealtimeAudioToTextWindow::~RealtimeAudioToTextWindow() {
    std::cout << "[DEBUG] RealtimeAudioToTextWindow destructor called" << std::endl;
    
    if (controller_) {
        std::cout << "[DEBUG] Calling controller shutdown..." << std::endl;
        controller_->shutdown();
        std::cout << "[DEBUG] Controller shutdown completed" << std::endl;
    }
    
    std::cout << "[DEBUG] RealtimeAudioToTextWindow destroyed." << std::endl;
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

    // 伪状态栏
    QHBoxLayout* statusLayout = new QHBoxLayout();
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("QLabel { color: #d0d0d0; }");
    statusLayout->addWidget(statusLabel_);
    mainLayout_->addLayout(statusLayout);

    mainLayout_->setStretch(0, 0); // menuBar
    mainLayout_->setStretch(1, 5); // textEdit_
    mainLayout_->setStretch(2, 0); // status bar
}

void RealtimeAudioToTextWindow::setupMenuBar() {}
void RealtimeAudioToTextWindow::setupStatusBar() {}

void RealtimeAudioToTextWindow::setupBottomControls() {
    QFrame* bottomFrame = new QFrame(this);
    bottomFrame->setStyleSheet(".QFrame { background-color: #2E2E2E; border-radius: 15px; }");
    
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
        "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #555555, stop:1 #333333);"
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
    if (!isRecording_) {
        // Start recording
        if (controller_->startRecording()) {
            isRecording_ = true;
            isPaused_ = false;
            recordingStartTime_ = QDateTime::currentMSecsSinceEpoch();
            timer_->start(100); // Update timer every 100ms
            updateStatusBar("录音中...");
            
            // 启动实时ASR
            if (controller_) {
                controller_->enableRealtimeAsr(true);
            }
        }
    } else {
        // Pause/Resume recording
        isPaused_ = !isPaused_;
        if (isPaused_) {
            controller_->pauseRecording();
            updateStatusBar("录音已暂停");
            
            // 暂停时禁用实时ASR
            if (controller_) {
                controller_->enableRealtimeAsr(false);
            }
        } else {
            controller_->resumeRecording();
            updateStatusBar("录音中...");
            
            // 恢复时重新启用实时ASR
            if (controller_) {
                controller_->enableRealtimeAsr(true);
            }
        }
    }
    updateRecordingButtons();
}

void RealtimeAudioToTextWindow::stopRecording() {
    std::cout << "[UI] stopRecording() called, isRecording_: " << isRecording_ << std::endl;
    if (!isRecording_ && !isPaused_) return;

    if (controller_) {
        controller_->stopRecording();
        // 停止录音时禁用ASR
        controller_->enableRealtimeAsr(false);
    }
    
    isRecording_ = false;
    isPaused_ = false;
    timer_->stop();
    updateStatusBar("待机中 - 选择音频源开始录音");
    updateRecordingButtons();
    
    // 清空UI和控制器状态
    clearTranscription();
    
    std::cout << "[Logic] Recording stopped." << std::endl;
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
    updateStatusBar("Transcript saved to " + fileName);
}

void RealtimeAudioToTextWindow::onDeviceSelectionResult(bool success, const QString& message) {
    updateStatusBar(message);
    if (!success) {
        QMessageBox::warning(this, "Device Error", message);
    }
}

void RealtimeAudioToTextWindow::onAudioSourceChanged(int index) {
    if (index < 0) return;
    int deviceId = audioSourceComboBox_->itemData(index).toInt();
    if (deviceId >= 0) {
        controller_->selectAudioDevice(deviceId);
    }
}

void RealtimeAudioToTextWindow::updateStatusBar(const QString& message) {
    if (statusLabel_) {
        statusLabel_->setText(message);
    }
}

void RealtimeAudioToTextWindow::closeEvent(QCloseEvent *event) {
    std::cout << "[DEBUG] RealtimeAudioToTextWindow closeEvent called" << std::endl;
    
    if (isRecording_) {
        QMessageBox::StandardButton res = QMessageBox::question(this, "退出确认",
            "录音正在进行中，您想在退出前停止并保存录音吗？",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);

        if (res == QMessageBox::Yes) {
            stopRecording(); // This will trigger the save dialog etc.
            // 关闭ASR
            if (controller_) {
                controller_->enableRealtimeAsr(false);
                controller_->shutdown();
            }
            event->accept();
        } else if (res == QMessageBox::No) {
            // User wants to discard the recording.
            // 关闭ASR
            if (controller_) {
                controller_->enableRealtimeAsr(false);
                controller_->shutdown();
            }
            event->accept();
        } else {
            // User cancelled the close operation.
            event->ignore();
            return;
        }
    } else {
        // 关闭ASR
        if (controller_) {
            controller_->enableRealtimeAsr(false);
            controller_->shutdown();
        }
        event->accept(); // Not recording, close normally.
    }
    
    std::cout << "[DEBUG] RealtimeAudioToTextWindow closeEvent completed" << std::endl;
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
    finalLines_.clear();
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        if (!line.isEmpty()) finalLines_ << line;
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
    QMessageBox::warning(this, "ASR Error", error);
    statusLabel_->setText("ASR: Error");
    statusLabel_->setStyleSheet("color: red;");
}

void RealtimeAudioToTextWindow::onAsrConnectionStatusChanged(bool connected) {
    statusLabel_->setText(connected ? "ASR: Connected" : "ASR: Disconnected");
    statusLabel_->setStyleSheet(connected ? "color: green;" : "color: red;");
}

void RealtimeAudioToTextWindow::onAsrUtterancesUpdated(const QList<QVariantMap>& utterances) {
    QStringList lines;
    QString previewLine;

    // 合并短句的最小长度
    const int minLineLen = 6;

    for (const auto& utter : utterances) {
        if (utter["definite"].toBool()) {
            QString txt = utter["text"].toString();
            if (txt.length() < minLineLen && !lines.isEmpty()) {
                // 合并到上一行
                lines.last() += txt;
            } else {
                lines << txt;
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

    QString displayText = lines.join("<br>");
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

void RealtimeAudioToTextWindow::backToMainMenu() {
    emit backToMainMenuRequested();
}

} // namespace ui
} // namespace perfx

#include "realtime_audio_to_text_window.moc" 
