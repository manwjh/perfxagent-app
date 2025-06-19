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
#include <memory>
#include <vector>
#include <string>
#include "audio/audio_manager.h"

namespace perfx {
namespace audio {
class FileImporter;
class AudioConverter;
}

namespace ui {

class AudioToTextWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit AudioToTextWindow(QWidget *parent = nullptr);
    ~AudioToTextWindow() override;

private slots:
    void importFile();
    void convertAudio();
    void setApiKey();
    void onOutputFileInfo(const QString& info);
    void onConversionProgress(int progress);
    void onConversionComplete(const QString& outputFile);
    void onError(const QString& errorMessage);

private:
    void setupUI();
    void setupMenuBar();
    void connectSignals();

    QTextEdit* textEdit_;
    QProgressBar* progressBar_;
    std::unique_ptr<audio::FileImporter> fileImporter_;
    std::unique_ptr<audio::AudioConverter> audioConverter_;
    std::vector<std::string> inputFiles_;
};

} // namespace ui
} // namespace perfx 