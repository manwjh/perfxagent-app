#pragma once
#include <QPushButton>
#include <QPropertyAnimation>

class AnimatedButton : public QPushButton {
    Q_OBJECT
public:
    explicit AnimatedButton(QWidget* parent = nullptr);

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void animateTo(const QPoint& pos, const QSize& size);

    QPoint originalPos_;
    QSize originalSize_;
    QPropertyAnimation* posAnim_;
    QPropertyAnimation* sizeAnim_;
    bool initialized_ = false;
}; 