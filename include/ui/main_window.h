#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>

namespace perfx {
namespace ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void setupUi();

private slots:
    void openAudioToTextWindow();
};

} // namespace ui
} // namespace perfx 