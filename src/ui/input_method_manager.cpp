#include "ui/input_method_manager.h"
#include <QApplication>
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDebug>
#include <QTimer>
#include <QList>

namespace perfx {
namespace ui {

InputMethodManager* InputMethodManager::instance_ = nullptr;

InputMethodManager::InputMethodManager(QObject* parent)
    : QObject(parent)
{
}

InputMethodManager* InputMethodManager::instance() {
    if (!instance_) {
        instance_ = new InputMethodManager();
    }
    return instance_;
}

void InputMethodManager::initialize() {
    // 设置环境变量
    setupEnvironmentVariables();
}

void InputMethodManager::setupEnvironmentVariables() {
    // 抑制IMK相关的警告信息
    qputenv("IMKCFRunLoopWakeUpReliable", "0");
    qputenv("QT_MAC_DISABLE_IMK", "0");
}

void InputMethodManager::optimizeWindow(QWidget* window) {
    if (!window) return;
    
    // 设置窗口属性以优化输入法处理
    window->setAttribute(Qt::WA_InputMethodEnabled, true);
    window->setAttribute(Qt::WA_InputMethodTransparent, false);
}

void InputMethodManager::optimizeInputWidget(QWidget* inputWidget) {
    if (!inputWidget) return;
    
    if (isInputWidget(inputWidget)) {
        applyInputWidgetOptimization(inputWidget);
    }
}

void InputMethodManager::optimizeAllInputWidgets(QWidget* window) {
    if (!window) return;
    
    optimizeInputWidgetsRecursive(window);
}

void InputMethodManager::optimizeInputWidgetsRecursive(QWidget* widget) {
    if (!widget) return;
    
    // 优化当前控件
    optimizeInputWidget(widget);
    
    // 递归优化子控件
    QList<QWidget*> children = widget->findChildren<QWidget*>();
    for (QWidget* child : children) {
        optimizeInputWidgetsRecursive(child);
    }
}

bool InputMethodManager::isInputWidget(QWidget* widget) {
    if (!widget) return false;
    
    // 检查是否为常见的输入控件类型
    return (qobject_cast<QLineEdit*>(widget) != nullptr ||
            qobject_cast<QTextEdit*>(widget) != nullptr ||
            qobject_cast<QPlainTextEdit*>(widget) != nullptr ||
            qobject_cast<QComboBox*>(widget) != nullptr ||
            qobject_cast<QSpinBox*>(widget) != nullptr ||
            qobject_cast<QDoubleSpinBox*>(widget) != nullptr);
}

void InputMethodManager::applyInputWidgetOptimization(QWidget* widget) {
    if (!widget) return;
    
    // 设置输入控件属性
    widget->setAttribute(Qt::WA_InputMethodEnabled, true);
    widget->setAttribute(Qt::WA_InputMethodTransparent, false);
}

} // namespace ui
} // namespace perfx 