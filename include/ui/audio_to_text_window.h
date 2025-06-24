#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QLabel>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSlider>
#include <QToolButton>
#include <QCloseEvent>
#include <memory>
#include <vector>
#include <string>
#include <QMap>
#include "audio/audio_manager.h"
#include "asr/asr_manager.h"

namespace perfx {
namespace audio {
class FileImporter;
class AudioConverter;
}

namespace ui {

class EnhancedAsrCallback : public QObject, public Asr::AsrCallback {
    Q_OBJECT
public:
    explicit EnhancedAsrCallback(QTextEdit* te);
    void onOpen(Asr::AsrClient* client) override;
    void onClose(Asr::AsrClient* client) override;
    void onMessage(Asr::AsrClient* client, const std::string& message) override;
    void onError(Asr::AsrClient* client, const std::string& error) override;
public slots:
    void clearText();
    void updateTextEdit(const QString& text, int sentenceIndex);
    void appendError(const QString& errorMsg);
Q_SIGNALS:
    void finished();
private:
    QTextEdit* textEdit_;
    QMap<int, QString> sentences_;
    QString currentText_;
    QString m_intermediateLine;
};

class AudioToTextWindow : public QWidget {
    Q_OBJECT

public:
    explicit AudioToTextWindow(QWidget *parent = nullptr);
    ~AudioToTextWindow() override;

signals:
    void backToMainMenuRequested();

private slots:
    void importFile();
    void asrTranscribe();
    void setApiKey();
    void onOutputFileInfo(const QString& info);
    void onConversionProgress(int progress);
    void onConversionComplete(const QString& outputFile);
    void onError(const QString& errorMessage);
    void onAsrFinished();
    void backToMainMenu();
    
    // 音频播放控制槽函数
    void playAudio();
    void pauseAudio();
    void stopAudio();
    void onPlaybackPositionChanged(qint64 position);
    void onPlaybackDurationChanged(qint64 duration);
    void onPlaybackSliderMoved(int position);
    void onMediaPlayerStateChanged(QMediaPlayer::PlaybackState state);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMediaPlayerError(QMediaPlayer::Error error, const QString& errorString);
    
    // 歌词相关槽函数
    void viewLyrics();
    void exportLrcFile();
    void exportJsonFile();
    void clearLyrics();
    void convertAudio();

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void setupAudioPlayerControls();
    void connectSignals();
    void updateStatusBar(const QString& message);
    void convertToWavFile(const std::string& inputFile, const std::string& outputFile);
    void updatePlaybackControls();
    QString formatTime(qint64 milliseconds);
    void startNextAsrTask();
    
    // 窗口事件处理
    void closeEvent(QCloseEvent *event) override;

    QTextEdit* textEdit_;
    QStatusBar* statusBar_;
    QLabel* statusLabel_;
    
    // 音频播放控制组件
    QMediaPlayer* mediaPlayer_;
    QAudioOutput* audioOutput_;
    QToolButton* playButton_;
    QToolButton* pauseButton_;
    QToolButton* stopButton_;
    QSlider* playbackSlider_;
    QLabel* timeLabel_;
    QHBoxLayout* playbackLayout_;
    
    std::unique_ptr<audio::FileImporter> fileImporter_;
    std::unique_ptr<audio::AudioConverter> audioConverter_;
    std::unique_ptr<Asr::AsrManager> asrManager_;
    std::unique_ptr<EnhancedAsrCallback> asrCallback_;
    std::vector<std::string> inputFiles_;
    std::vector<std::string> workFiles_;  // 工作文件（转换后的WAV文件）
    std::vector<std::string>::iterator currentWorkFile_;
    
    bool hasWorkFiles_;  // 是否有工作文件
};

} // namespace ui
} // namespace perfx 