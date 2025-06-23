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
    , currentAudioToTextWindow_(nullptr)
    , currentRealtimeAudioToTextWindow_(nullptr)
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
        "  background-color: #FF8C00;"
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
    // Check if any sub window is currently open
    if (currentAudioToTextWindow_ || currentRealtimeAudioToTextWindow_) {
        // If a sub window is still open, don't allow switching
        // You could show a message to the user here if needed
        return;
    }
    
    currentAudioToTextWindow_ = new AudioToTextWindow(this);
    currentAudioToTextWindow_->setAttribute(Qt::WA_DeleteOnClose);
    
    // Connect the window's destroyed signal to reset our pointer
    connect(currentAudioToTextWindow_, &AudioToTextWindow::destroyed, 
            this, &MainWindow::onSubWindowClosed);
    
    currentAudioToTextWindow_->show();
}

void MainWindow::openRealtimeAudioToTextWindow() {
    // Check if any sub window is currently open
    if (currentAudioToTextWindow_ || currentRealtimeAudioToTextWindow_) {
        // If a sub window is still open, don't allow switching
        // You could show a message to the user here if needed
        return;
    }
    
    currentRealtimeAudioToTextWindow_ = new RealtimeAudioToTextWindow(this);
    currentRealtimeAudioToTextWindow_->setAttribute(Qt::WA_DeleteOnClose);
    
    // Connect the window's destroyed signal to reset our pointer
    connect(currentRealtimeAudioToTextWindow_, &RealtimeAudioToTextWindow::destroyed, 
            this, &MainWindow::onSubWindowClosed);
    
    currentRealtimeAudioToTextWindow_->show();
}

void MainWindow::onSubWindowClosed() {
    // Reset pointers when sub windows are closed
    if (sender() == currentAudioToTextWindow_) {
        currentAudioToTextWindow_ = nullptr;
    } else if (sender() == currentRealtimeAudioToTextWindow_) {
        currentRealtimeAudioToTextWindow_ = nullptr;
    }
}

MainWindow::~MainWindow() = default;

} // namespace ui
} // namespace perfx 