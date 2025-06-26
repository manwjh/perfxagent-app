#pragma once

#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QEvent>
#include <QEnterEvent>

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
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QLabel* iconLabel_;
    QLabel* textLabel_;
};

} // namespace ui
} // namespace perfx 