#ifndef INPUT_METHOD_MANAGER_H
#define INPUT_METHOD_MANAGER_H

#include <QObject>
#include <QWidget>
#include <QApplication>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDebug>
#include <QTimer>

namespace perfx {
namespace ui {

class InputMethodManager : public QObject {
    Q_OBJECT

public:
    static InputMethodManager* instance();
    
    // 初始化输入法管理器
    void initialize();
    
    // 为窗口应用输入法优化
    void optimizeWindow(QWidget* window);
    
    // 为输入控件应用输入法优化
    void optimizeInputWidget(QWidget* inputWidget);
    
    // 批量优化窗口中的所有输入控件
    void optimizeAllInputWidgets(QWidget* window);
    
    // 设置环境变量
    void setupEnvironmentVariables();

private:
    InputMethodManager(QObject* parent = nullptr);
    ~InputMethodManager() = default;
    
    static InputMethodManager* instance_;
    
    // 递归查找并优化所有输入控件
    void optimizeInputWidgetsRecursive(QWidget* widget);
    
    // 检查是否为输入控件
    bool isInputWidget(QWidget* widget);
    
    // 应用具体的优化设置
    void applyInputWidgetOptimization(QWidget* widget);
};

} // namespace ui
} // namespace perfx

#endif // INPUT_METHOD_MANAGER_H 