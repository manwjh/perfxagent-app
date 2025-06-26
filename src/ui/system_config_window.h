#ifndef SYSTEM_CONFIG_WINDOW_H
#define SYSTEM_CONFIG_WINDOW_H

#include <QWidget>

class SystemConfigWindow : public QWidget
{
public:
    SystemConfigWindow();

private:
    QHBoxLayout* buttonLayout_;
};

#endif 