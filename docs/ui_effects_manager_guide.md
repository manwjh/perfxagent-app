# UI特效管理器使用指南

## 概述

`UIEffectsManager` 是一个统一的UI特效管理系统，集成了多种常用的UI动画和视觉效果功能。它提供了模块化的设计，使得UI特效的创建和使用变得简单和一致。

## 主要功能

### 1. 动画按钮 (AnimatedButton)

继承自 `QPushButton`，提供丰富的动画效果：

```cpp
#include "ui/ui_effects_manager.h"

using namespace perfx::ui;

// 创建默认动画按钮
UIEffectsManager::AnimatedButton* button = 
    UIEffectsManager::Utils::createAnimatedButton("点击我", parent);

// 自定义动画参数
button->setHoverOffset(QPoint(0, -5));      // 悬停时向上偏移5像素
button->setHoverScale(QSize(10, 10));       // 悬停时放大10x10像素
button->setPressOffset(QPoint(0, 2));       // 按下时向下偏移2像素
button->setPressScale(QSize(-6, -6));       // 按下时缩小6x6像素
button->setAnimationDuration(200);          // 设置动画持续时间为200ms
```

### 2. 图标按钮 (IconButton)

带阴影和动画效果的图标按钮：

```cpp
// 创建默认图标按钮
QIcon icon = QIcon(":/icons/app_icon_64x64.png");
UIEffectsManager::IconButton* iconButton = 
    UIEffectsManager::Utils::createIconButton(icon, "图标按钮", parent);

// 自定义属性
iconButton->setIconSize(QSize(32, 32));           // 设置图标大小
iconButton->setButtonSize(QSize(80, 100));        // 设置按钮大小
iconButton->setShadowEffect(true, 20, 5);         // 启用阴影，模糊半径20，偏移5
iconButton->setHoverScale(1.1);                   // 悬停时放大1.1倍
```

### 3. 通用动画效果 (AnimationEffect)

提供各种类型的动画效果：

```cpp
// 位置动画
UIEffectsManager::AnimationEffect::animatePosition(widget, QPoint(100, 100), 500);

// 大小动画
UIEffectsManager::AnimationEffect::animateSize(widget, QSize(200, 150), 500);

// 透明度动画
UIEffectsManager::AnimationEffect::animateOpacity(widget, 0.5, 500);

// 缩放动画
UIEffectsManager::AnimationEffect::animateScale(widget, 1.2, 500);

// 旋转动画
UIEffectsManager::AnimationEffect::animateRotation(widget, 45.0, 500);

// 组合动画 - 悬停效果
UIEffectsManager::AnimationEffect::animateHoverEffect(
    widget, QPoint(0, -2), QSize(6, 6), 200);

// 组合动画 - 点击效果
UIEffectsManager::AnimationEffect::animateClickEffect(
    widget, QPoint(0, 1), QSize(-4, -4), 100);
```

### 4. 阴影效果 (ShadowEffect)

提供可配置的阴影效果：

```cpp
// 创建阴影效果
QGraphicsDropShadowEffect* shadow = UIEffectsManager::ShadowEffect::createShadow(
    15, QColor(0, 0, 0, 60), QPoint(0, 3));

// 应用阴影到控件
UIEffectsManager::ShadowEffect::applyShadow(widget, 15, QColor(0, 0, 0, 60), QPoint(0, 3));

// 移除阴影
UIEffectsManager::ShadowEffect::removeShadow(widget);
```

### 5. 工具函数 (Utils)

提供常用的UI效果组合：

```cpp
// 创建带动画效果的按钮
UIEffectsManager::AnimatedButton* button = 
    UIEffectsManager::Utils::createAnimatedButton("按钮", parent);

// 创建带阴影的图标按钮
UIEffectsManager::IconButton* iconButton = 
    UIEffectsManager::Utils::createIconButton(icon, "图标", parent);

// 应用悬停效果
UIEffectsManager::Utils::applyHoverEffect(widget, QPoint(0, -2), QSize(6, 6), 200);

// 应用点击效果
UIEffectsManager::Utils::applyClickEffect(widget, QPoint(0, 1), QSize(-4, -4), 100);
```

## 使用示例

### 基本使用

```cpp
#include "ui/ui_effects_manager.h"
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>

using namespace perfx::ui;

class MainWindow : public QMainWindow {
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        QVBoxLayout* layout = new QVBoxLayout(centralWidget);
        
        // 创建动画按钮
        UIEffectsManager::AnimatedButton* button = 
            UIEffectsManager::Utils::createAnimatedButton("动画按钮", this);
        layout->addWidget(button);
        
        // 创建图标按钮
        QIcon icon = QIcon(":/icons/app_icon_64x64.png");
        UIEffectsManager::IconButton* iconButton = 
            UIEffectsManager::Utils::createIconButton(icon, "图标按钮", this);
        layout->addWidget(iconButton);
        
        // 连接信号
        connect(button, &QPushButton::clicked, this, &MainWindow::onButtonClicked);
        connect(iconButton, &UIEffectsManager::IconButton::clicked, 
                this, &MainWindow::onIconButtonClicked);
    }

private slots:
    void onButtonClicked() {
        qDebug() << "动画按钮被点击";
    }
    
    void onIconButtonClicked() {
        qDebug() << "图标按钮被点击";
    }
};
```

### 自定义动画效果

```cpp
// 为任意控件添加悬停效果
void addHoverEffect(QWidget* widget) {
    widget->installEventFilter(this);
}

bool eventFilter(QObject* obj, QEvent* event) override {
    if (obj->isWidgetType()) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (event->type() == QEvent::Enter) {
            UIEffectsManager::Utils::applyHoverEffect(widget);
        } else if (event->type() == QEvent::Leave) {
            // 恢复原始状态
            UIEffectsManager::AnimationEffect::animatePosition(widget, widget->pos(), 200);
            UIEffectsManager::AnimationEffect::animateSize(widget, widget->size(), 200);
        }
    }
    return QMainWindow::eventFilter(obj, event);
}
```

## 配置选项

### 动画按钮配置

| 参数 | 默认值 | 说明 |
|------|--------|------|
| hoverOffset | QPoint(0, -2) | 悬停时的位置偏移 |
| hoverScale | QSize(6, 6) | 悬停时的大小变化 |
| pressOffset | QPoint(0, 1) | 按下时的位置偏移 |
| pressScale | QSize(-4, -4) | 按下时的大小变化 |
| animationDuration | 100 | 动画持续时间(毫秒) |

### 图标按钮配置

| 参数 | 默认值 | 说明 |
|------|--------|------|
| iconSize | QSize(48, 48) | 图标大小 |
| buttonSize | QSize(100, 120) | 按钮大小 |
| shadowEnabled | true | 是否启用阴影 |
| shadowBlurRadius | 15 | 阴影模糊半径 |
| shadowOffset | 3 | 阴影偏移 |
| hoverScale | 1.05 | 悬停时缩放比例 |

## 最佳实践

1. **性能考虑**：避免在动画过程中进行复杂的计算或频繁的DOM操作
2. **用户体验**：保持动画时长在100-300ms之间，确保流畅自然
3. **一致性**：在整个应用中使用统一的动画参数，保持视觉一致性
4. **可访问性**：考虑为动画提供开关选项，满足不同用户的需求

## 注意事项

1. 确保在Qt项目中正确包含了相关的头文件和库
2. 动画效果依赖于Qt的动画框架，确保Qt版本支持
3. 阴影效果在某些平台上可能有性能影响，请根据实际情况调整
4. 建议在调试模式下测试动画效果，确保在不同设备上的表现一致

## 扩展开发

如果需要添加新的动画效果，可以：

1. 在 `AnimationEffect` 类中添加新的静态方法
2. 在 `Utils` 类中添加便捷的包装函数
3. 创建新的自定义控件类继承自现有类

```cpp
// 示例：添加新的动画效果
void UIEffectsManager::AnimationEffect::animateBounce(QWidget* widget, int duration) {
    // 实现弹跳动画效果
    QPropertyAnimation* anim = new QPropertyAnimation(widget, "pos");
    anim->setDuration(duration);
    anim->setStartValue(widget->pos());
    anim->setEndValue(widget->pos() + QPoint(0, -20));
    anim->setEasingCurve(QEasingCurve::OutBounce);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}
``` 