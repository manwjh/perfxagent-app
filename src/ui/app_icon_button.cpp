#include "ui/app_icon_button.h"
#include <QVBoxLayout>
#include <QIcon>
#include <QLabel>

namespace perfx {
namespace ui {

AppIconButton::AppIconButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QWidget(parent) {
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);

    iconLabel_ = new QLabel(this);
    iconLabel_->setPixmap(icon.pixmap(64, 64)); // Icon size
    iconLabel_->setAlignment(Qt::AlignCenter);

    textLabel_ = new QLabel(text, this);
    textLabel_->setAlignment(Qt::AlignCenter);
    textLabel_->setStyleSheet("color: #333;");

    layout->addWidget(iconLabel_);
    layout->addWidget(textLabel_);
    layout->addStretch();
    
    setLayout(layout);
    setFixedSize(80, 90);
}

AppIconButton::~AppIconButton() = default;

void AppIconButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

} // namespace ui
} // namespace perfx 