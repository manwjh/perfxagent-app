#pragma once

#include <QMainWindow>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>

namespace perfx {
namespace ui {

class HelloWindow : public QMainWindow {
    Q_OBJECT
public:
    enum class ButtonState { WAITING, RECORDING, PLAYING };
    explicit HelloWindow(QWidget* parent = nullptr);
    ~HelloWindow() = default;

private:
    void setupUI();
    void onAgentChanged(int index);
    void onButtonPressed();
    void onButtonReleased();
    void onPlaybackFinished();
    void updateButtonState(const QString& state);

    QComboBox* m_agentSelector;
    QPushButton* m_recordButton;
    QTextEdit* m_chatWindow;
    QTimer* m_playbackTimer;
    ButtonState m_currentState;
};

} // namespace ui
} // namespace perfx 