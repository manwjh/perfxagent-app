#include "ui/hello_window.h"
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>

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
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Agent selector
    auto* selectorLayout = new QHBoxLayout;
    m_agentSelector = new QComboBox(this);
    m_agentSelector->addItems({"xiaozhi", "perfxchat", "recoder"});
    m_agentSelector->setStyleSheet(R"(
        QComboBox {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
            min-width: 150px;
        }
        QComboBox:focus {
            border: 1px solid #00a0ff;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
    )");
    selectorLayout->addWidget(m_agentSelector);
    selectorLayout->addStretch();
    mainLayout->addLayout(selectorLayout);
    
    // Record button
    m_recordButton = new QPushButton("☕", this);
    m_recordButton->setFixedSize(100, 100);
    m_recordButton->setStyleSheet(R"(
        QPushButton {
            font-size: 24px;
            border-radius: 50px;
            background-color: #007bff;
            color: white;
            border: none;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_recordButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    // Chat window
    m_chatWindow = new QTextEdit(this);
    m_chatWindow->setReadOnly(true);
    m_chatWindow->setMinimumHeight(200);
    m_chatWindow->setStyleSheet(R"(
        QTextEdit {
            background-color: #2d2d2d;
            color: white;
            border: 1px solid #007bff;
            border-radius: 8px;
            padding: 8px;
        }
        QScrollBar:vertical {
            border: none;
            background: #1e1e1e;
            width: 10px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #007bff;
            min-height: 20px;
            border-radius: 5px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
    )");
    mainLayout->addWidget(m_chatWindow);
    
    // Input area
    auto* inputLayout = new QHBoxLayout;
    m_inputEdit = new QLineEdit(this);
    m_inputEdit->setPlaceholderText("输入消息...");
    m_inputEdit->setStyleSheet(R"(
        QLineEdit {
            padding: 8px;
            border: 1px solid #007bff;
            border-radius: 8px;
            background-color: #2d2d2d;
            color: white;
        }
        QLineEdit:focus {
            border: 1px solid #00a0ff;
        }
    )");
    
    m_sendButton = new QPushButton("发送", this);
    m_sendButton->setStyleSheet(R"(
        QPushButton {
            padding: 8px 16px;
            background-color: #007bff;
            color: white;
            border: none;
            border-radius: 8px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #0056b3;
        }
        QPushButton:pressed {
            background-color: #004085;
        }
    )");
    
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(m_sendButton);
    mainLayout->addLayout(inputLayout);
    
    // Playback timer
    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setSingleShot(true);
    connect(m_playbackTimer, &QTimer::timeout, this, &HelloWindow::onPlaybackFinished);
    
    // Connect signals
    connect(m_sendButton, &QPushButton::clicked, this, &HelloWindow::onSendMessage);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &HelloWindow::onSendMessage);
    
    setWindowTitle("Hello AI");
    resize(400, 600);
}

void HelloWindow::onSendMessage()
{
    QString message = m_inputEdit->text().trimmed();
    if (!message.isEmpty()) {
        m_chatWindow->append("<p style='color: #007bff;'>我: " + message + "</p>");
        m_inputEdit->clear();
        // TODO: 处理消息发送逻辑
    }
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