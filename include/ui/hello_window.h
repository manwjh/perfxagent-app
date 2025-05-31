#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QLineEdit>

namespace perfx {
namespace ui {

class HelloWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class ButtonState { WAITING, RECORDING, PLAYING };
    explicit HelloWindow(QWidget* parent = nullptr);
    ~HelloWindow() = default;

private slots:
    void onAgentChanged(int index);
    void onButtonPressed();
    void onButtonReleased();
    void onPlaybackFinished();
    void onSendMessage();

private:
    void setupUI();
    void updateButtonState(const QString& state);

    QComboBox* m_agentSelector;
    QPushButton* m_recordButton;
    QTextEdit* m_chatWindow;
    QTimer* m_playbackTimer;
    ButtonState m_currentState;
    QLineEdit* m_inputEdit;
    QPushButton* m_sendButton;
};

} // namespace ui
} // namespace perfx 