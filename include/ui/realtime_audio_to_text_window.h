#pragma once

#include <QWidget>
#include <memory>
#include <QMap>

// Forward declarations
class QCloseEvent;
class QTextEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QFrame;
class QTimer;
class QStatusBar;
class QVBoxLayout;

namespace perfx {
namespace logic {
class RealtimeTranscriptionController;
}
namespace ui {

class WaveformWidget;

class RealtimeAudioToTextWindow : public QWidget {
    Q_OBJECT

public:
    explicit RealtimeAudioToTextWindow(QWidget *parent = nullptr);
    ~RealtimeAudioToTextWindow();

    // 麦克风管理方法
    void startMicCollection();
    void stopMicCollection();

    signals:
    void backToMainMenuRequested();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Recording controls
    void toggleRecording();
    void stopRecording();
    void saveRecording();
    void backToMainMenu();
    
    // UI updates
    void updateTimerDisplay();
    void updateAudioDeviceList(const QStringList& names, const QList<int>& ids);
    void updateWaveform(const QVector<float>& samples);

    // Event handlers
    void onDeviceSelectionResult(bool success, const QString& message);
    void onAudioSourceChanged(int index);
    
    // ASR signal handlers
    void onAsrTranscriptionUpdated(const QString& text, bool isFinal);
    void onAsrError(const QString& error);
    void onAsrConnectionStatusChanged(bool connected);
    void onAsrUtterancesUpdated(const QList<QVariantMap>& utterances);
    
    // Menu actions
    void clearText();
    void downloadText();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupBottomControls();
    void connectSignals();

    void updateRecordingButtons();
    void updateStatusBar(const QString& message);
    void updateStatusBarSuccess(const QString& message);
    void updateStatusBarWarning(const QString& message);
    void updateStatusBarError(const QString& message);
    void clearTranscription();
    QIcon createRedDotIcon();

    // UI Elements
    QTextEdit* textEdit_;
    QLabel* statusLabel_;
    QPushButton* recordPauseButton_;
    QPushButton* stopButton_;
    QLabel* timerLabel_;
    WaveformWidget* waveformWidget_;
    QComboBox* audioSourceComboBox_;
    QStatusBar* statusBar_;
    QFrame* waveformFrame_;
    QVBoxLayout* mainLayout_;
    
    // State
    std::unique_ptr<logic::RealtimeTranscriptionController> controller_;
    QTimer* timer_;
    bool isRecording_;
    bool isPaused_;
    qint64 recordingStartTime_;
    
    // Text handling state
    QString cumulativeText_;
    QString partialText_;
    QString lastSavedDirectory_;
    QMap<int, QString> sentenceMap_;
    QStringList finalLines_;
};

} // namespace ui
} // namespace perfx 