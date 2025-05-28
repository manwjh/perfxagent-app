#include "ui/hello_window.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>

namespace perfx {
namespace ui {

HelloWindow::HelloWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_currentState(ButtonState::WAITING)
{
    setupUI();
}

void HelloWindow::setupUI()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    auto* mainLayout = new QVBoxLayout(centralWidget);
    
    // Agent selector
    m_agentSelector = new QComboBox(this);
    m_agentSelector->addItems({"xiaozhi", "perfxchat", "recoder"});
    connect(m_agentSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HelloWindow::onAgentChanged);
    mainLayout->addWidget(m_agentSelector);
    
    // Record button
    m_recordButton = new QPushButton("☕", this);
    m_recordButton->setFixedSize(100, 100);
    m_recordButton->setStyleSheet("QPushButton { font-size: 24px; }");
    connect(m_recordButton, &QPushButton::pressed, this, &HelloWindow::onButtonPressed);
    connect(m_recordButton, &QPushButton::released, this, &HelloWindow::onButtonReleased);
    
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    // Chat window
    m_chatWindow = new QTextEdit(this);
    m_chatWindow->setReadOnly(true);
    m_chatWindow->setMinimumHeight(200);
    mainLayout->addWidget(m_chatWindow);
    
    // Playback timer
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setSingleShot(true);
    connect(m_playbackTimer, &QTimer::timeout, this, &HelloWindow::onPlaybackFinished);
    
    setWindowTitle("Hello AI");
    resize(400, 600);
}

void HelloWindow::onAgentChanged(int index)
{
    (void)index;
    // TODO: Implement agent change logic
}

void HelloWindow::onButtonPressed()
{
    m_currentState = ButtonState::RECORDING;
    updateButtonState("🔴");
    // TODO: Implement recording logic
}

void HelloWindow::onButtonReleased()
{
    m_currentState = ButtonState::PLAYING;
    updateButtonState("🔊");
    // TODO: Implement playback logic
    m_playbackTimer->start(5000); // 5 seconds playback simulation
}

void HelloWindow::onPlaybackFinished()
{
    m_currentState = ButtonState::WAITING;
    updateButtonState("☕");
}

void HelloWindow::updateButtonState(const QString& state)
{
    m_recordButton->setText(state);
}

} // namespace ui
} // namespace perfx 