#include <QStyle>
#include "ui/main_window.h"
#include "ui/audio_to_text_window.h"
#include "ui/realtime_audio_to_text_window.h"
#include "ui/app_icon_button.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>

namespace perfx {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // Make the window frameless and transparent
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
}

void MainWindow::setupUi() {
    setWindowTitle("PerfxAgent Hello...");
    resize(390, 844); // iPhone-like dimensions

    // Main container with rounded corners and gradient
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    centralWidget->setStyleSheet(
        "#centralWidget {"
        "  background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #FDEB71, stop: 1 #F8D800);"
        "  border-radius: 30px;"
        "}"
    );
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Custom Title Bar
    QWidget* titleBar = new QWidget(this);
    titleBar->setFixedHeight(40);
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    
    QPushButton* minimizeButton = new QPushButton("-", this);
    QPushButton* closeButton = new QPushButton("x", this);
    minimizeButton->setFixedSize(20, 20);
    closeButton->setFixedSize(20, 20);

    titleLayout->addStretch();
    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(closeButton);

    mainLayout->addWidget(titleBar);

    connect(closeButton, &QPushButton::clicked, this, &MainWindow::close);
    connect(minimizeButton, &QPushButton::clicked, this, &MainWindow::showMinimized);
    
    // Grid for app icons
    QGridLayout* gridLayout = new QGridLayout();
    mainLayout->addLayout(gridLayout);
    mainLayout->setContentsMargins(20, 10, 20, 20);

    // Create and add app icons
    AppIconButton* audioToTextBtn = new AppIconButton(
        style()->standardIcon(QStyle::SP_FileIcon), "文件转录", this);
    connect(audioToTextBtn, &AppIconButton::clicked, this, &MainWindow::openAudioToTextWindow);
    gridLayout->addWidget(audioToTextBtn, 0, 0, Qt::AlignTop);
    
    AppIconButton* realtimeAudioToTextBtn = new AppIconButton(
        style()->standardIcon(QStyle::SP_ComputerIcon), "实时转录", this);
    connect(realtimeAudioToTextBtn, &AppIconButton::clicked, this, &MainWindow::openRealtimeAudioToTextWindow);
    gridLayout->addWidget(realtimeAudioToTextBtn, 0, 1, Qt::AlignTop);

    mainLayout->addStretch(); // Pushes icons to the top
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragPosition_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition_);
        event->accept();
    }
}

void MainWindow::openAudioToTextWindow() {
    AudioToTextWindow* window = new AudioToTextWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

void MainWindow::openRealtimeAudioToTextWindow() {
    RealtimeAudioToTextWindow* window = new RealtimeAudioToTextWindow(this);
    window->setAttribute(Qt::WA_DeleteOnClose);
    window->show();
}

MainWindow::~MainWindow() = default;

} // namespace ui
} // namespace perfx 