#pragma once

#include <QWidget>
#include <QLabel>
#include <QMouseEvent>

namespace perfx {
namespace ui {

class AppIconButton : public QWidget {
    Q_OBJECT

public:
    explicit AppIconButton(const QIcon& icon, const QString& text, QWidget* parent = nullptr);
    ~AppIconButton() override;

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QLabel* iconLabel_;
    QLabel* textLabel_;
};

} // namespace ui
} // namespace perfx 