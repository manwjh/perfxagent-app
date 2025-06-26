#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMouseEvent>
#include <QStackedWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QEvent>

namespace perfx {
namespace ui {

class AudioToTextWindow;
class RealtimeAudioToTextWindow;
class SystemConfigWindow;
class ConfigManager;

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
    void setupTitleBar(QVBoxLayout* mainLayout);
    void createMainMenuPage();
    void createFeatureButton(const QString& title, const QString& description, 
                           const QIcon& icon, QGridLayout* layout, 
                           int row, int col, QObject* receiver, 
                           void (MainWindow::*slot)());
    void loadAsrConfig();

private slots:
    void switchToAudioToText();
    void switchToRealtimeAudioToText();
    void switchToSystemConfig();
    void switchToMainMenu();
    void onConfigUpdated();
    
private:
    QPoint dragPosition_;
    QStackedWidget* stackedWidget_;
    AudioToTextWindow* audioToTextWindow_;
    RealtimeAudioToTextWindow* realtimeAudioToTextWindow_;
    SystemConfigWindow* systemConfigWindow_;
    QWidget* mainMenuWidget_;
    
    // ASR配置相关
    ConfigManager* configManager_;
    bool asrConfigLoaded_;
    bool asrConfigValid_;
};

} // namespace ui
} // namespace perfx 