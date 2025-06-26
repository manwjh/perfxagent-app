#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QFrame>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStyle>
#include <QFont>
#include <QFontMetrics>

namespace perfx {
namespace ui {

/**
 * @brief 关于项目组件
 * 
 * 显示项目的基本信息，包括软件名称、介绍、作者、联系方式等
 * 提供可点击的链接到GitHub和邮箱
 */
class AboutProjectWidget : public QWidget {
    Q_OBJECT

public:
    explicit AboutProjectWidget(QWidget* parent = nullptr);
    ~AboutProjectWidget();

private slots:
    void onGithubLinkClicked();
    void onEmailLinkClicked();

private:
    void setupUI();
    void setupProjectInfo();
    void setupLinks();
    void setupStyle();
    bool eventFilter(QObject* obj, QEvent* event) override;
    
    // UI组件
    QVBoxLayout* mainLayout_;
    QFrame* projectInfoGroup_;
    QFrame* linksGroup_;
    
    // 项目信息标签
    QLabel* softwareNameLabel_;
    QLabel* descriptionLabel_;
    QLabel* versionLabel_;
    QLabel* authorLabel_;
    
    // 链接标签
    QLabel* githubLinkLabel_;
    QLabel* emailLinkLabel_;
    
    // 项目信息
    struct ProjectInfo {
        QString softwareName = "PerfXAgent-ASR";
        QString description = "这是一款关于AI音频应用的程序，99%的代码由AI生成";
        QString version = "1.6.2";
        QString author = "深圳王哥";
        QString githubUrl = "https://github.com/manwjh/perfxagent-app";
        QString email = "manwjh@126.com";
    };
    
    ProjectInfo projectInfo_;
};

} // namespace perfx
} // namespace ui 