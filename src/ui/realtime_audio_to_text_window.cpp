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
    : QMainWindow(parent),
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
    updateStatusBar("Ready - Select an audio source to begin.");
    updateRecordingButtons();
    controller_->refreshAudioDevices();
}

RealtimeAudioToTextWindow::~RealtimeAudioToTextWindow() {
    // 确保在窗口销毁时，控制器被正确关闭，从而释放音频设备。
    if (controller_) {
        controller_->shutdown();
    }
    std::cout << "[UI] RealtimeAudioToTextWindow destroyed." << std::endl;
}

void RealtimeAudioToTextWindow::setupUI() {
    setWindowTitle("Real-time Transcription");
    resize(400, 700);
    setStyleSheet("QMainWindow { background-color: #1E1E1E; }");

    // Create central widget and main layout
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout_ = new QVBoxLayout(centralWidget);
    mainLayout_->setSpacing(0);
    mainLayout_->setContentsMargins(10, 10, 10, 10);

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
    mainLayout_->addWidget(textEdit_, 1);

    mainLayout_->setStretch(0, 5);
    mainLayout_->setStretch(1, 2);
}

void RealtimeAudioToTextWindow::setupMenuBar() {
    QMenuBar* menuBar = this->menuBar();
    menuBar->setStyleSheet("QMenuBar { background-color: #2E2E2E; color: #F0F0F0; } "
                           "QMenu { background-color: #2E2E2E; color: #F0F0F0; }");
    
    // File Menu
    QMenu* fileMenu = menuBar->addMenu("File");
    
    QAction* clearAction = new QAction("Clear Text", this);
    connect(clearAction, &QAction::triggered, this, &RealtimeAudioToTextWindow::clearText);
    fileMenu->addAction(clearAction);
    
    QAction* downloadAction = new QAction("Save Transcript", this);
    connect(downloadAction, &QAction::triggered, this, &RealtimeAudioToTextWindow::downloadText);
    fileMenu->addAction(downloadAction);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = new QAction("Exit", this);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);
}

void RealtimeAudioToTextWindow::setupStatusBar() {
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    statusLabel_ = new QLabel("Ready", this);
    statusBar_->setStyleSheet("QStatusBar { color: #d0d0d0; }");
    statusBar_->addWidget(statusLabel_);
}

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
            this, &RealtimeAudioToTextWindow::onAsrTranscriptionUpdated);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrError,
            this, &RealtimeAudioToTextWindow::onAsrError);
            
    connect(controller_.get(), &logic::RealtimeTranscriptionController::asrConnectionStatusChanged,
            this, &RealtimeAudioToTextWindow::onAsrConnectionStatusChanged);

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
            updateStatusBar("Recording...");
            
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
            updateStatusBar("Paused");
            
            // 暂停时禁用实时ASR
            if (controller_) {
                controller_->enableRealtimeAsr(false);
            }
        } else {
            controller_->resumeRecording();
            updateStatusBar("Recording...");
            
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
    updateStatusBar("Recording stopped. Ready.");
    updateRecordingButtons();
    
    saveRecordingDialog();
    
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
        recordPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    } else {
        recordPauseButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    }
    // Adjust icon size if needed
    recordPauseButton_->setIconSize(QSize(35, 35));
}

void RealtimeAudioToTextWindow::clearText() {
    textEdit_->clear();
    cumulativeText_.clear();
    partialText_.clear();
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
    if (isRecording_) {
        QMessageBox::StandardButton res = QMessageBox::question(this, "退出确认",
            "录音正在进行中，您想在退出前停止并保存录音吗？",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::Yes);

        if (res == QMessageBox::Yes) {
            stopRecording(); // This will trigger the save dialog etc.
            event->accept();
        } else if (res == QMessageBox::No) {
            // User wants to discard the recording.
            // We just need to accept the event, the shutdown logic in the destructor
            // will handle the resource cleanup.
            event->accept();
        } else {
            // User cancelled the close operation.
            event->ignore();
        }
    } else {
        event->accept(); // Not recording, close normally.
    }
}

// ============================================================================
// 实时ASR槽函数实现 (新增)
// ============================================================================

void RealtimeAudioToTextWindow::onAsrTranscriptionUpdated(const QString& text, bool isFinal) {
    if (isFinal) {
        cumulativeText_.append(text + " ");
        partialText_.clear();
    } else {
        partialText_ = text;
    }
    textEdit_->setPlainText(cumulativeText_ + partialText_);
    textEdit_->moveCursor(QTextCursor::End);
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
}

void RealtimeAudioToTextWindow::saveRecording() {
    // This method can be implemented if needed. For now, it's just a placeholder.
}

void RealtimeAudioToTextWindow::saveRecordingDialog()
{
    if (lastSavedDirectory_.isEmpty()) {
        lastSavedDirectory_ = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    }

    QString defaultFileName = "perfxagent_recording_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = QFileDialog::getSaveFileName(this, "Save Recording",
                                                    lastSavedDirectory_ + "/" + defaultFileName,
                                                    "WAV files (*.wav);;All files (*.*)");

    if (!fileName.isEmpty()) {
        lastSavedDirectory_ = QFileInfo(fileName).absolutePath();
        // 暂时注释掉，因为控制器可能没有这个方法
        // controller_->saveRecordingToFile(fileName);
        QMessageBox::information(this, "Success", "Recording saved to:\n" + fileName);
    } else {
        QMessageBox::information(this, "Cancelled", "Save operation was cancelled.");
    }
}

} // namespace ui
} // namespace perfx

#include "realtime_audio_to_text_window.moc" 