#include <QApplication>
#include <QResource>
#include <QDir>
#include <QDebug>
#include "../include/ui/main_window.h"

int main(int argc, char *argv[])
{
    qDebug() << "QResource test:" << QResource(":/resources/icons/audio_file.png").isValid();
    QDir dir(":/resources/icons");
    qDebug() << "Resource list:" << dir.entryList();
    try {
        QApplication app(argc, argv);
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