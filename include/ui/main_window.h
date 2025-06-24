#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMouseEvent>
#include <QStackedWidget>

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
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void createMainMenuPage();

private slots:
    void switchToAudioToText();
    void switchToRealtimeAudioToText();
    void switchToMainMenu();
    
private:
    QPoint dragPosition_;
    QStackedWidget* stackedWidget_;
    AudioToTextWindow* audioToTextWindow_;
    RealtimeAudioToTextWindow* realtimeAudioToTextWindow_;
    QWidget* mainMenuWidget_;
};

} // namespace ui
} // namespace perfx 