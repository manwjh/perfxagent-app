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
};

} // namespace ui
} // namespace perfx 