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
#include <QtWidgets/QStackedWidget>

namespace perfx {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , stackedWidget_(nullptr)
    , audioToTextWindow_(nullptr)
    , realtimeAudioToTextWindow_(nullptr)
    , mainMenuWidget_(nullptr)
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
    
    // Create stacked widget for different pages
    stackedWidget_ = new QStackedWidget(this);
    mainLayout->addWidget(stackedWidget_);
    
    // Create main menu page
    createMainMenuPage();
    
    // Create sub-windows as pages
    audioToTextWindow_ = new AudioToTextWindow(this);
    realtimeAudioToTextWindow_ = new RealtimeAudioToTextWindow(this);
    
    // Add pages to stacked widget
    stackedWidget_->addWidget(mainMenuWidget_);
    stackedWidget_->addWidget(audioToTextWindow_);
    stackedWidget_->addWidget(realtimeAudioToTextWindow_);
    
    // Connect back to main menu signals
    connect(audioToTextWindow_, &AudioToTextWindow::backToMainMenuRequested, 
            this, &MainWindow::switchToMainMenu);
    connect(realtimeAudioToTextWindow_, &RealtimeAudioToTextWindow::backToMainMenuRequested, 
            this, &MainWindow::switchToMainMenu);
    
    // Start with main menu
    stackedWidget_->setCurrentWidget(mainMenuWidget_);
}

void MainWindow::createMainMenuPage() {
    mainMenuWidget_ = new QWidget(this);
    QVBoxLayout* mainMenuLayout = new QVBoxLayout(mainMenuWidget_);
    mainMenuLayout->setContentsMargins(20, 10, 20, 20);

    // Grid for app icons
    QGridLayout* gridLayout = new QGridLayout();
    mainMenuLayout->addLayout(gridLayout);

    // Create and add app icons
    AppIconButton* audioToTextBtn = new AppIconButton(
        style()->standardIcon(QStyle::SP_FileIcon), "文件转录", this);
    connect(audioToTextBtn, &AppIconButton::clicked, this, &MainWindow::switchToAudioToText);
    gridLayout->addWidget(audioToTextBtn, 0, 0, Qt::AlignTop);
    
    AppIconButton* realtimeAudioToTextBtn = new AppIconButton(
        style()->standardIcon(QStyle::SP_ComputerIcon), "实时转录", this);
    connect(realtimeAudioToTextBtn, &AppIconButton::clicked, this, &MainWindow::switchToRealtimeAudioToText);
    gridLayout->addWidget(realtimeAudioToTextBtn, 0, 1, Qt::AlignTop);

    mainMenuLayout->addStretch(); // Pushes icons to the top
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

void MainWindow::switchToAudioToText() {
    stackedWidget_->setCurrentWidget(audioToTextWindow_);
}

void MainWindow::switchToRealtimeAudioToText() {
    stackedWidget_->setCurrentWidget(realtimeAudioToTextWindow_);
}

void MainWindow::switchToMainMenu() {
    stackedWidget_->setCurrentWidget(mainMenuWidget_);
}

MainWindow::~MainWindow() = default;

} // namespace ui
} // namespace perfx 