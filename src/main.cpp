#include <QApplication>
#include <QResource>
#include <QDir>
#include <QDebug>
#include "../include/ui/main_window.h"
#include "../include/asr/asr_manager.h"
#include <QStyleFactory>
#include <QStandardPaths>
#include <QSettings>
#include <QMessageBox>
#include <QProcess>
#include <iostream>
#include <cstdlib>
#include "../include/ui/input_method_manager.h"

int main(int argc, char *argv[]) {
    // 设置环境变量
    qputenv("IMKCFRunLoopWakeUpReliable", "0");
    qputenv("QT_MAC_DISABLE_IMK", "0");
    
    QApplication app(argc, argv);
    
    // 全局样式表：统一弹窗、按钮、标签风格
    app.setStyleSheet(
        "QMessageBox { background: #f5f6fa; }"
        "QLabel { color: #222; font-size: 18px; font-weight: 600; }"
        "QPushButton {"
        "  background: #FF8C00;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 8px 24px;"
        "  font-size: 16px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover { background: #FF6B35; }"
    );
    
    // 初始化输入法管理器
    perfx::ui::InputMethodManager::instance()->initialize();
    
    try {
        perfx::ui::MainWindow w;
        w.show();
        return app.exec();
    } catch (const std::exception& e) {
        fprintf(stderr, "[FATAL] main exception: %s\n", e.what());
        return 1;
    } catch (...) {
        fprintf(stderr, "[FATAL] main unknown exception!\n");
        return 2;
    }
} 