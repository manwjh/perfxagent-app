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
    // 在QApplication创建之前设置环境变量
    qputenv("IMKCFRunLoopWakeUpReliable", "0");
    qputenv("QT_MAC_WANTS_LAYER", "1");
    qputenv("QT_MAC_DISABLE_IMK", "0");
    
    // 在macOS上设置额外的属性（移除已弃用的高DPI属性）
#ifdef Q_OS_MAC
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus, false);
    QApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta, true);
#endif
    
    QApplication app(argc, argv);
    
    // 初始化输入法管理器
    perfx::ui::InputMethodManager::instance()->initialize();
    
    qDebug() << "QResource test:" << QResource(":/resources/icons/audio_file.png").isValid();
    QDir dir(":/resources/icons");
    qDebug() << "Resource list:" << dir.entryList();
    
    // 测试ASR管理器单例模式
    qDebug() << "=== 测试ASR管理器单例模式 ===";
    Asr::AsrManager& instance1 = Asr::AsrManager::instance();
    Asr::AsrManager& instance2 = Asr::AsrManager::instance();
    qDebug() << "实例1地址:" << &instance1;
    qDebug() << "实例2地址:" << &instance2;
    qDebug() << "是否为同一实例:" << (&instance1 == &instance2 ? "是" : "否");
    qDebug() << "=== 单例模式测试完成 ===";
    
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