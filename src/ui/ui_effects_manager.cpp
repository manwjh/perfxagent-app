#include "ui/ui_effects_manager.h"
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QApplication>
#include <QScreen>

namespace perfx {
namespace ui {

// ============================================================================
// AnimatedButton 实现
// ============================================================================

AnimatedButton::AnimatedButton(QWidget* parent)
    : QPushButton(parent),
      posAnim_(new QPropertyAnimation(this, "pos")),
      sizeAnim_(new QPropertyAnimation(this, "size"))
{
    posAnim_->setDuration(animationDuration_);
    sizeAnim_->setDuration(animationDuration_);
}

void AnimatedButton::setHoverOffset(const QPoint& offset) {
    hoverOffset_ = offset;
}

void AnimatedButton::setHoverScale(const QSize& scale) {
    hoverScale_ = scale;
}

void AnimatedButton::setPressOffset(const QPoint& offset) {
    pressOffset_ = offset;
}

void AnimatedButton::setPressScale(const QSize& scale) {
    pressScale_ = scale;
}

void AnimatedButton::setAnimationDuration(int duration) {
    animationDuration_ = duration;
    posAnim_->setDuration(duration);
    sizeAnim_->setDuration(duration);
}

void AnimatedButton::enterEvent(QEnterEvent* event) {
    if (!initialized_) {
        originalPos_ = pos();
        originalSize_ = size();
        initialized_ = true;
    }
    animateTo(originalPos_ + hoverOffset_, originalSize_ + hoverScale_);
    QPushButton::enterEvent(event);
}

void AnimatedButton::leaveEvent(QEvent* event) {
    animateTo(originalPos_, originalSize_);
    QPushButton::leaveEvent(event);
}

void AnimatedButton::mousePressEvent(QMouseEvent* event) {
    animateTo(originalPos_ + pressOffset_, originalSize_ + pressScale_);
    QPushButton::mousePressEvent(event);
}

void AnimatedButton::mouseReleaseEvent(QMouseEvent* event) {
    animateTo(originalPos_, originalSize_);
    QPushButton::mouseReleaseEvent(event);
}

void AnimatedButton::animateTo(const QPoint& pos, const QSize& size) {
    posAnim_->stop();
    sizeAnim_->stop();
    posAnim_->setStartValue(this->pos());
    posAnim_->setEndValue(pos);
    sizeAnim_->setStartValue(this->size());
    sizeAnim_->setEndValue(size);
    posAnim_->start();
    sizeAnim_->start();
}

// ============================================================================
// IconButton 实现
// ============================================================================

IconButton::IconButton(const QIcon& icon, const QString& text, QWidget* parent)
    : QWidget(parent), shadowEffect_(nullptr) {
    
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // 创建图标容器
    iconContainer_ = new QFrame(this);
    iconContainer_->setFixedSize(80, 80);
    iconContainer_->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.1);"
        "  border: 1px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 20px;"
        "  padding: 15px;"
        "}"
    );
    
    // 添加阴影效果
    updateShadowEffect();
    
    QVBoxLayout* iconLayout = new QVBoxLayout(iconContainer_);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    iconLabel_ = new QLabel(this);
    iconLabel_->setPixmap(icon.pixmap(iconSize_));
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setStyleSheet(
        "QLabel {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    iconLayout->addWidget(iconLabel_);
    
    layout->addWidget(iconContainer_, 0, Qt::AlignCenter);

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
    setFixedSize(buttonSize_);
    
    // 设置整体样式
    setStyleSheet(
        "QWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
}

IconButton::~IconButton() = default;

void IconButton::setIconSize(const QSize& size) {
    iconSize_ = size;
    if (iconLabel_) {
        QPixmap currentPixmap = iconLabel_->pixmap();
        if (!currentPixmap.isNull()) {
            iconLabel_->setPixmap(currentPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void IconButton::setButtonSize(const QSize& size) {
    buttonSize_ = size;
    setFixedSize(size);
}

void IconButton::setShadowEffect(bool enabled, int blurRadius, int offset) {
    shadowEnabled_ = enabled;
    shadowBlurRadius_ = blurRadius;
    shadowOffset_ = offset;
    updateShadowEffect();
}

void IconButton::setHoverScale(qreal scale) {
    hoverScale_ = scale;
}

void IconButton::updateShadowEffect() {
    if (shadowEffect_) {
        iconContainer_->setGraphicsEffect(nullptr);
        delete shadowEffect_;
        shadowEffect_ = nullptr;
    }
    
    if (shadowEnabled_) {
        shadowEffect_ = new QGraphicsDropShadowEffect();
        shadowEffect_->setBlurRadius(shadowBlurRadius_);
        shadowEffect_->setColor(QColor(0, 0, 0, 60));
        shadowEffect_->setOffset(shadowOffset_);
        iconContainer_->setGraphicsEffect(shadowEffect_);
    }
}

void IconButton::applyHoverEffect(bool hover) {
    if (hover) {
        iconContainer_->setStyleSheet(
            "QFrame {"
            "  background: rgba(255, 255, 255, 0.15);"
            "  border: 1px solid rgba(255, 255, 255, 0.3);"
            "  border-radius: 20px;"
            "  padding: 15px;"
            "  transform: scale(" + QString::number(hoverScale_) + ");"
            "}"
        );
    } else {
        iconContainer_->setStyleSheet(
            "QFrame {"
            "  background: rgba(255, 255, 255, 0.1);"
            "  border: 1px solid rgba(255, 255, 255, 0.2);"
            "  border-radius: 20px;"
            "  padding: 15px;"
            "}"
        );
    }
}

void IconButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
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

void IconButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        setStyleSheet(
            "QWidget {"
            "  background: transparent;"
            "  border: none;"
            "}"
        );
    }
    QWidget::mouseReleaseEvent(event);
}

void IconButton::enterEvent(QEnterEvent* event) {
    setCursor(Qt::PointingHandCursor);
    applyHoverEffect(true);
    QWidget::enterEvent(event);
}

void IconButton::leaveEvent(QEvent* event) {
    setCursor(Qt::ArrowCursor);
    applyHoverEffect(false);
    QWidget::leaveEvent(event);
}

// ============================================================================
// AnimationEffect 实现
// ============================================================================

void AnimationEffect::animatePosition(QWidget* widget, const QPoint& endPos, int duration) {
    QPropertyAnimation* anim = new QPropertyAnimation(widget, "pos");
    anim->setDuration(duration);
    anim->setStartValue(widget->pos());
    anim->setEndValue(endPos);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationEffect::animateSize(QWidget* widget, const QSize& endSize, int duration) {
    QPropertyAnimation* anim = new QPropertyAnimation(widget, "size");
    anim->setDuration(duration);
    anim->setStartValue(widget->size());
    anim->setEndValue(endSize);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationEffect::animateOpacity(QWidget* widget, qreal endOpacity, int duration) {
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    
    QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(duration);
    anim->setStartValue(effect->opacity());
    anim->setEndValue(endOpacity);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationEffect::animateScale(QWidget* widget, qreal endScale, int duration) {
    // 简化实现：使用大小动画来模拟缩放效果
    QSize currentSize = widget->size();
    QSize newSize = QSize(currentSize.width() * endScale, currentSize.height() * endScale);
    animateSize(widget, newSize, duration);
}

void AnimationEffect::animateRotation(QWidget* widget, qreal endRotation, int duration) {
    // 简化实现：暂时不实现旋转动画
    Q_UNUSED(widget)
    Q_UNUSED(endRotation)
    Q_UNUSED(duration)
}

void AnimationEffect::animateHoverEffect(QWidget* widget, const QPoint& offset, const QSize& scale, int duration) {
    QPoint originalPos = widget->pos();
    QSize originalSize = widget->size();
    
    QPropertyAnimation* posAnim = new QPropertyAnimation(widget, "pos");
    posAnim->setDuration(duration);
    posAnim->setStartValue(originalPos);
    posAnim->setEndValue(originalPos + offset);
    
    QPropertyAnimation* sizeAnim = new QPropertyAnimation(widget, "size");
    sizeAnim->setDuration(duration);
    sizeAnim->setStartValue(originalSize);
    sizeAnim->setEndValue(originalSize + scale);
    
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
    sizeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void AnimationEffect::animateClickEffect(QWidget* widget, const QPoint& offset, const QSize& scale, int duration) {
    QPoint originalPos = widget->pos();
    QSize originalSize = widget->size();
    
    QPropertyAnimation* posAnim = new QPropertyAnimation(widget, "pos");
    posAnim->setDuration(duration);
    posAnim->setStartValue(originalPos);
    posAnim->setEndValue(originalPos + offset);
    
    QPropertyAnimation* sizeAnim = new QPropertyAnimation(widget, "size");
    sizeAnim->setDuration(duration);
    sizeAnim->setStartValue(originalSize);
    sizeAnim->setEndValue(originalSize + scale);
    
    posAnim->start(QAbstractAnimation::DeleteWhenStopped);
    sizeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ============================================================================
// ShadowEffect 实现
// ============================================================================

QGraphicsDropShadowEffect* ShadowEffect::createShadow(
    int blurRadius, const QColor& color, const QPoint& offset) {
    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
    effect->setBlurRadius(blurRadius);
    effect->setColor(color);
    effect->setOffset(offset.x(), offset.y());
    return effect;
}

void ShadowEffect::applyShadow(QWidget* widget, int blurRadius, const QColor& color, const QPoint& offset) {
    QGraphicsDropShadowEffect* effect = createShadow(blurRadius, color, offset);
    widget->setGraphicsEffect(effect);
}

void ShadowEffect::removeShadow(QWidget* widget) {
    widget->setGraphicsEffect(nullptr);
}

// ============================================================================
// UIEffectsUtils 实现
// ============================================================================

AnimatedButton* UIEffectsUtils::createAnimatedButton(const QString& text, QWidget* parent) {
    AnimatedButton* button = new AnimatedButton(parent);
    button->setText(text);
    return button;
}

IconButton* UIEffectsUtils::createIconButton(const QIcon& icon, const QString& text, QWidget* parent) {
    return new IconButton(icon, text, parent);
}

void UIEffectsUtils::applyHoverEffect(QWidget* widget, const QPoint& offset, const QSize& scale, int duration) {
    AnimationEffect::animateHoverEffect(widget, offset, scale, duration);
}

void UIEffectsUtils::applyClickEffect(QWidget* widget, const QPoint& offset, const QSize& scale, int duration) {
    AnimationEffect::animateClickEffect(widget, offset, scale, duration);
}

} // namespace ui
} // namespace perfx 