#include "ui/main_window.h"
#include "ui/audio_to_text_window.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

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

    // 创建中央部件
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // 添加录音转文本按钮
    QPushButton* audioToTextBtn = new QPushButton("录音转文本", this);
    connect(audioToTextBtn, &QPushButton::clicked, this, &MainWindow::openAudioToTextWindow);
    mainLayout->addWidget(audioToTextBtn);
    mainLayout->addStretch();
}

void MainWindow::openAudioToTextWindow() {
    AudioToTextWindow* window = new AudioToTextWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

MainWindow::~MainWindow() {}

} // namespace ui
} // namespace perfx 