#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMouseEvent>

namespace perfx {
namespace ui {

class AudioToTextWindow;
class RealtimeAudioToTextWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void setupUi();

private slots:
    void openAudioToTextWindow();
    void openRealtimeAudioToTextWindow();
    void onSubWindowClosed();
    
private:
    QPoint dragPosition_;
    AudioToTextWindow* currentAudioToTextWindow_;
    RealtimeAudioToTextWindow* currentRealtimeAudioToTextWindow_;
};

} // namespace ui
} // namespace perfx 