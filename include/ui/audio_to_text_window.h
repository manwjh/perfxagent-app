#pragma once

#include <QMainWindow>
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
#include <memory>
#include <vector>
#include <string>
#include "audio/audio_manager.h"
#include "asr/asr_manager.h"

namespace perfx {
namespace audio {
class FileImporter;
class AudioConverter;
}

namespace ui {

class EnhancedAsrCallback;

class AudioToTextWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AudioToTextWindow(QWidget *parent = nullptr);
    ~AudioToTextWindow() override;

private slots:
    void importFile();
    void asrTranscribe();
    void setApiKey();
    void onOutputFileInfo(const QString& info);
    void onConversionProgress(int progress);
    void onConversionComplete(const QString& outputFile);
    void onError(const QString& errorMessage);
    void onAsrFinished();
    
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