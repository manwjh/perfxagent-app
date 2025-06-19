#include <QApplication>
#include "../include/ui/main_window.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    perfx::ui::MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
} 