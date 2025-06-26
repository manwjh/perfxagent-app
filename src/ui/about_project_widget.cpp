#include "ui/about_project_widget.h"
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QStyle>
#include <QFont>
#include <QFontMetrics>
#include <QMessageBox>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

namespace perfx {
namespace ui {

AboutProjectWidget::AboutProjectWidget(QWidget* parent)
    : QWidget(parent)
    , mainLayout_(nullptr)
    , projectInfoGroup_(nullptr)
    , linksGroup_(nullptr)
    , softwareNameLabel_(nullptr)
    , descriptionLabel_(nullptr)
    , versionLabel_(nullptr)
    , authorLabel_(nullptr)
    , githubLinkLabel_(nullptr)
    , emailLinkLabel_(nullptr)
{
    setupUI();
    setupProjectInfo();
    setupLinks();
    setupStyle();
}

AboutProjectWidget::~AboutProjectWidget() = default;

void AboutProjectWidget::setupUI() {
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(20);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    
    // 项目信息组
    projectInfoGroup_ = new QFrame();
    projectInfoGroup_->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.05);"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 12px;"
        "  padding: 20px;"
        "}"
    );
    
    QVBoxLayout* projectLayout = new QVBoxLayout(projectInfoGroup_);
    projectLayout->setSpacing(15);
    projectLayout->setContentsMargins(20, 20, 20, 20);
    
    // 软件名称
    softwareNameLabel_ = new QLabel();
    softwareNameLabel_->setAlignment(Qt::AlignCenter);
    softwareNameLabel_->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    projectLayout->addWidget(softwareNameLabel_);
    
    // 软件介绍
    descriptionLabel_ = new QLabel();
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setAlignment(Qt::AlignCenter);
    descriptionLabel_->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.8);"
        "  font-size: 13px;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  line-height: 1.4;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 10px;"
        "}"
    );
    projectLayout->addWidget(descriptionLabel_);
    
    // 版本信息
    versionLabel_ = new QLabel();
    versionLabel_->setAlignment(Qt::AlignCenter);
    versionLabel_->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.7);"
        "  font-size: 12px;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    projectLayout->addWidget(versionLabel_);
    
    mainLayout_->addWidget(projectInfoGroup_);
    
    // 链接组
    linksGroup_ = new QFrame();
    linksGroup_->setStyleSheet(
        "QFrame {"
        "  background: rgba(255, 255, 255, 0.05);"
        "  border: 1px solid rgba(255, 255, 255, 0.1);"
        "  border-radius: 12px;"
        "  padding: 20px;"
        "}"
    );
    
    QVBoxLayout* linksLayout = new QVBoxLayout(linksGroup_);
    linksLayout->setSpacing(15);
    linksLayout->setContentsMargins(20, 20, 20, 20);
    
    // 链接标题
    QLabel* linksTitle = new QLabel("🔗 联系方式");
    linksTitle->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    linksTitle->setAlignment(Qt::AlignCenter);
    linksLayout->addWidget(linksTitle);
    
    // GitHub链接
    githubLinkLabel_ = new QLabel();
    githubLinkLabel_->setAlignment(Qt::AlignCenter);
    githubLinkLabel_->setCursor(Qt::PointingHandCursor);
    githubLinkLabel_->setStyleSheet(
        "QLabel {"
        "  color: #58a6ff;"
        "  font-size: 12px;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 8px;"
        "  border-radius: 6px;"
        "}"
        "QLabel:hover {"
        "  background: rgba(88, 166, 255, 0.1);"
        "  color: #79c0ff;"
        "}"
    );
    linksLayout->addWidget(githubLinkLabel_);
    
    // 邮箱链接
    emailLinkLabel_ = new QLabel();
    emailLinkLabel_->setAlignment(Qt::AlignCenter);
    emailLinkLabel_->setCursor(Qt::PointingHandCursor);
    emailLinkLabel_->setStyleSheet(
        "QLabel {"
        "  color: #58a6ff;"
        "  font-size: 12px;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 8px;"
        "  border-radius: 6px;"
        "}"
        "QLabel:hover {"
        "  background: rgba(88, 166, 255, 0.1);"
        "  color: #79c0ff;"
        "}"
    );
    linksLayout->addWidget(emailLinkLabel_);
    
    mainLayout_->addWidget(linksGroup_);
    
    // 安装事件过滤器
    githubLinkLabel_->installEventFilter(this);
    emailLinkLabel_->installEventFilter(this);
}

void AboutProjectWidget::setupProjectInfo() {
    softwareNameLabel_->setText(projectInfo_.softwareName);
    descriptionLabel_->setText(projectInfo_.description);
    versionLabel_->setText(QString("版本: %1").arg(projectInfo_.version));
}

void AboutProjectWidget::setupLinks() {
    githubLinkLabel_->setText(QString("📱 GitHub: %1").arg(projectInfo_.githubUrl));
    emailLinkLabel_->setText(QString("📧 邮箱: %1").arg(projectInfo_.email));
}

void AboutProjectWidget::setupStyle() {
    setStyleSheet(
        "QWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
}

bool AboutProjectWidget::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == githubLinkLabel_) {
            onGithubLinkClicked();
            return true;
        } else if (obj == emailLinkLabel_) {
            onEmailLinkClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AboutProjectWidget::onGithubLinkClicked() {
    QDesktopServices::openUrl(QUrl(projectInfo_.githubUrl));
}

void AboutProjectWidget::onEmailLinkClicked() {
    QDesktopServices::openUrl(QUrl(QString("mailto:%1").arg(projectInfo_.email)));
}

} // namespace perfx
} // namespace ui 