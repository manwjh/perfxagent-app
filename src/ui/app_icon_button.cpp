#include "ui/app_icon_button.h"
#include <QVBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>

namespace perfx {
namespace ui {

AppIconButton::AppIconButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QWidget(parent) {
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // 创建图标容器
    QFrame* iconContainer = new QFrame(this);
    iconContainer->setFixedSize(80, 80);
    iconContainer->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.1);"
        "  border: 1px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 20px;"
        "  padding: 15px;"
        "}"
        "QFrame:hover {"
        "  background: rgba(255, 255, 255, 0.15);"
        "  border-color: rgba(255, 255, 255, 0.3);"
        "  transform: scale(1.05);"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setBlurRadius(15);
    shadowEffect->setColor(QColor(0, 0, 0, 60));
    shadowEffect->setOffset(0, 3);
    iconContainer->setGraphicsEffect(shadowEffect);
    
    QVBoxLayout* iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    iconLabel_ = new QLabel(this);
    iconLabel_->setPixmap(icon.pixmap(48, 48));
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setStyleSheet(
        "QLabel {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    iconLayout->addWidget(iconLabel_);
    
    layout->addWidget(iconContainer, 0, Qt::AlignCenter);

    // 文本标签
    textLabel_ = new QLabel(text, this);
    textLabel_->setAlignment(Qt::AlignCenter);
    textLabel_->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 12px;"
        "  font-weight: 500;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 5px;"
        "}"
    );
    layout->addWidget(textLabel_);
    
    setLayout(layout);
    setFixedSize(100, 120);
    
    // 设置整体样式
    setStyleSheet(
        "QWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
}

AppIconButton::~AppIconButton() = default;

void AppIconButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 添加点击动画效果
        setStyleSheet(
            "QWidget {"
            "  background: transparent;"
            "  border: none;"
            "  transform: scale(0.95);"
            "}"
        );
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void AppIconButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 恢复原始样式
        setStyleSheet(
            "QWidget {"
            "  background: transparent;"
            "  border: none;"
            "}"
        );
    }
    QWidget::mouseReleaseEvent(event);
}

void AppIconButton::enterEvent(QEnterEvent* event) {
    // 鼠标进入时的效果
    setCursor(Qt::PointingHandCursor);
    QWidget::enterEvent(event);
}

void AppIconButton::leaveEvent(QEvent* event) {
    // 鼠标离开时的效果
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}

} // namespace ui
} // namespace perfx 