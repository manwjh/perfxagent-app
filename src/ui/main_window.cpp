#include "ui/main_window.h"
#include <QtWidgets/QVBoxLayout>

namespace perfx {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setupUi();
}

void MainWindow::setupUi() {
    setWindowTitle("PerfxAgent Hello...");
    resize(1024, 768);
}

MainWindow::~MainWindow() {}

} // namespace ui
} // namespace perfx 