#include <QApplication>
#include "ui/main_window.h"
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}

#include "main.moc" 