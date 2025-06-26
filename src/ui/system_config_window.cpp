#include "ui/system_config_window.h"
#include "ui/about_project_widget.h"
#include "ui/config_manager.h"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>
#include <QTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QProcessEnvironment>
#include <QDebug>
#include <QScrollArea>
#include <QFrame>
#include "asr/asr_manager.h"
#include "ui/global_state.h"
#include <iostream>

namespace perfx {
namespace ui {

SystemConfigWindow::SystemConfigWindow(QWidget* parent)
    : QWidget(parent)
    , mainLayout_(nullptr)
    , scrollArea_(nullptr)
    , contentWidget_(nullptr)
    , asrConfigGroup_(nullptr)
    , aboutGroup_(nullptr)
    , appIdEdit_(nullptr)
    , accessTokenEdit_(nullptr)
    , secretKeyEdit_(nullptr)
    , showPasswordBtn_(nullptr)
    , aboutWidget_(nullptr)
    , saveBtn_(nullptr)
    , testBtn_(nullptr)
    , guideBtn_(nullptr)
    , backBtn_(nullptr)
    , statusLabel_(nullptr)
    , testProgressBar_(nullptr)
    , configManager_(nullptr)
    , configModified_(false)
    , statusTimer_(nullptr)
    , buttonLayout_(nullptr)
{
    // 获取配置管理器实例
    configManager_ = ConfigManager::instance();
    
    setupUI();
    
    // 设置状态定时器 - 移到loadCurrentConfig之前
    statusTimer_ = new QTimer(this);
    statusTimer_->setSingleShot(true);
    connect(statusTimer_, &QTimer::timeout, [this]() {
        statusLabel_->clear();
        statusLabel_->setStyleSheet("");
    });
    
    loadCurrentConfig();
    
    // 连接配置管理器信号
    connect(configManager_, &ConfigManager::configUpdated, 
            this, &SystemConfigWindow::onConfigManagerUpdated);
    
    showStatusMessage("配置已加载，请根据需要修改参数");
}

SystemConfigWindow::~SystemConfigWindow() = default;

void SystemConfigWindow::setupUI() {
    // 设置窗口属性
    setWindowTitle("系统配置");
    setFixedSize(600, 700); // 增加窗口大小以适应新设计
    
    // 主布局
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(0);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    
    // 设置整体样式（深色风格）
    setStyleSheet(
        "QWidget, QFrame, QGroupBox, QScrollArea, QComboBox, QProgressBar {"
        "  background-color: #1E1E1E;"
        "  color: #F0F0F0;"
        "  border: none;"
        "  font-size: 16px;"
        "}"
        "QLabel {"
        "  color: #F0F0F0;"
        "  font-size: 16px;"
        "  background: transparent;"
        "}"
        "QLineEdit {"
        "  background-color: #232323;"
        "  color: #F0F0F0;"
        "  border: 1.5px solid #444;"
        "  border-radius: 8px;"
        "  padding: 10px 14px;"
        "  font-size: 16px;"
        "}"
        "QLineEdit:focus {"
        "  border: 1.5px solid #FF8C00;"
        "  background: #232323;"
        "}"
        "QLineEdit::placeholder {"
        "  color: #888888;"
        "  font-style: italic;"
        "}"
        "QPushButton {"
        "  background-color: #232323;"
        "  color: #F0F0F0;"
        "  border-radius: 10px;"
        "  padding: 10px 20px;"
        "  font-size: 16px;"
        "  border: none;"
        "}"
        "QPushButton:hover {"
        "  background-color: #333333;"
        "}"
        "QComboBox {"
        "  background-color: #232323;"
        "  color: #F0F0F0;"
        "  border-radius: 8px;"
        "  border: 1.5px solid #444;"
        "  padding: 8px 14px;"
        "  font-size: 16px;"
        "}"
        "QComboBox QAbstractItemView {"
        "  background-color: #232323;"
        "  color: #F0F0F0;"
        "}"
        "QProgressBar {"
        "  background-color: #232323;"
        "  color: #F0F0F0;"
        "  border-radius: 4px;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #FF8C00;"
        "  border-radius: 4px;"
        "}"
    );
    
    // 创建滚动区域
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // 内容容器
    contentWidget_ = new QWidget();
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setSpacing(20);
    contentLayout->setContentsMargins(30, 30, 30, 30);
    
    // 创建各个配置区域
    setupHeaderSection(contentLayout);
    setupAsrConfigSection(contentLayout);
    setupAboutSection(contentLayout);
    
    scrollArea_->setWidget(contentWidget_);
    mainLayout_->addWidget(scrollArea_);
    
    // 底部按钮区域
    setupBottomSection();
    
    // 状态区域
    setupStatusSection();
}

void SystemConfigWindow::setupHeaderSection(QVBoxLayout* contentLayout) {
    // 标题区域
    QFrame* headerFrame = new QFrame();
    headerFrame->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #FF8C00, stop:1 #FF6B35);"
        "  border-radius: 15px;"
        "  padding: 20px;"
        "}"
    );
    
    QVBoxLayout* headerLayout = new QVBoxLayout(headerFrame);
    headerLayout->setContentsMargins(25, 25, 25, 25);
    
    QLabel* titleLabel = new QLabel("系统配置");
    titleLabel->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  margin-bottom: 5px;"
        "}"
    );
    titleLabel->setAlignment(Qt::AlignCenter);
    
    QLabel* subtitleLabel = new QLabel("配置ASR服务和查看项目信息");
    subtitleLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.9);"
        "  font-size: 14px;"
        "  font-weight: normal;"
        "}"
    );
    subtitleLabel->setAlignment(Qt::AlignCenter);
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subtitleLabel);
    
    contentLayout->addWidget(headerFrame);
}

void SystemConfigWindow::setupAsrConfigSection(QVBoxLayout* contentLayout) {
    asrConfigGroup_ = new QFrame();
    asrConfigGroup_->setStyleSheet(
        "QFrame {"
        "  background: white;"
        "  border: 1px solid #e1e5e9;"
        "  border-radius: 15px;"
        "  padding: 25px;"
        "}"
    );
    
    QVBoxLayout* groupLayout = new QVBoxLayout(asrConfigGroup_);
    groupLayout->setSpacing(20);
    
    // 标题
    QLabel* groupTitle = new QLabel("🔧 ASR服务配置");
    groupTitle->setStyleSheet(
        "QLabel {"
        "  color: #2c3e50;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  margin-bottom: 10px;"
        "}"
    );
    groupLayout->addWidget(groupTitle);
    
    // 配置说明
    QLabel* descriptionLabel = new QLabel(
        "配置火山引擎ASR服务，支持语音转文字功能。请确保您已申请相关API密钥。"
    );
    descriptionLabel->setStyleSheet(
        "QLabel {"
        "  color: #6c757d;"
        "  font-size: 13px;"
        "  line-height: 1.4;"
        "  padding: 10px;"
        "  background: #f8f9fa;"
        "  border-radius: 8px;"
        "  border-left: 4px solid #FF8C00;"
        "}"
    );
    descriptionLabel->setWordWrap(true);
    groupLayout->addWidget(descriptionLabel);
    
    // 配置表单
    QGridLayout* formLayout = new QGridLayout();
    formLayout->setSpacing(15);
    
    int row = 0;
    
    // 应用ID
    QLabel* appIdLabel = new QLabel("应用ID (App ID)");
    appIdLabel->setStyleSheet(
        "QLabel {"
        "  color: #495057;"
        "  font-weight: 600;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addWidget(appIdLabel, row, 0);
    
    appIdEdit_ = new QLineEdit();
    appIdEdit_->setPlaceholderText("请输入您的应用ID");
    appIdEdit_->setMinimumHeight(45);
    appIdEdit_->setStyleSheet(
        "QLineEdit {"
        "  border: 2px solid #e9ecef;"
        "  border-radius: 10px;"
        "  padding: 12px 15px;"
        "  background: white;"
        "  font-size: 14px;"
        "  color: #495057;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #FF8C00;"
        "  background: #fff8f0;"
        "  box-shadow: 0 0 0 3px rgba(255, 140, 0, 0.1);"
        "}"
        "QLineEdit::placeholder {"
        "  color: #adb5bd;"
        "  font-style: italic;"
        "}"
    );
    formLayout->addWidget(appIdEdit_, row, 1);
    row++;
    
    // 访问令牌
    QLabel* accessTokenLabel = new QLabel("访问令牌 (Access Token)");
    accessTokenLabel->setStyleSheet(
        "QLabel {"
        "  color: #495057;"
        "  font-weight: 600;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addWidget(accessTokenLabel, row, 0);
    
    accessTokenEdit_ = new QLineEdit();
    accessTokenEdit_->setPlaceholderText("请输入您的访问令牌");
    accessTokenEdit_->setEchoMode(QLineEdit::Password);
    accessTokenEdit_->setMinimumHeight(45);
    accessTokenEdit_->setStyleSheet(
        "QLineEdit {"
        "  border: 2px solid #e9ecef;"
        "  border-radius: 10px;"
        "  padding: 12px 15px;"
        "  background: white;"
        "  font-size: 14px;"
        "  color: #495057;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #FF8C00;"
        "  background: #fff8f0;"
        "  box-shadow: 0 0 0 3px rgba(255, 140, 0, 0.1);"
        "}"
        "QLineEdit::placeholder {"
        "  color: #adb5bd;"
        "  font-style: italic;"
        "}"
    );
    formLayout->addWidget(accessTokenEdit_, row, 1);
    row++;
    
    // 密钥
    QLabel* secretKeyLabel = new QLabel("密钥 (Secret Key)");
    secretKeyLabel->setStyleSheet(
        "QLabel {"
        "  color: #495057;"
        "  font-weight: 600;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addWidget(secretKeyLabel, row, 0);
    
    secretKeyEdit_ = new QLineEdit();
    secretKeyEdit_->setPlaceholderText("请输入您的密钥");
    secretKeyEdit_->setEchoMode(QLineEdit::Password);
    secretKeyEdit_->setMinimumHeight(45);
    secretKeyEdit_->setStyleSheet(
        "QLineEdit {"
        "  border: 2px solid #e9ecef;"
        "  border-radius: 10px;"
        "  padding: 12px 15px;"
        "  background: white;"
        "  font-size: 14px;"
        "  color: #495057;"
        "}"
        "QLineEdit:focus {"
        "  border-color: #FF8C00;"
        "  background: #fff8f0;"
        "  box-shadow: 0 0 0 3px rgba(255, 140, 0, 0.1);"
        "}"
        "QLineEdit::placeholder {"
        "  color: #adb5bd;"
        "  font-style: italic;"
        "}"
    );
    formLayout->addWidget(secretKeyEdit_, row, 1);
    row++;
    
    // 显示/隐藏密码按钮
    showPasswordBtn_ = new QPushButton("👁 显示密码");
    showPasswordBtn_->setMaximumWidth(140);
    showPasswordBtn_->setMinimumHeight(40);
    showPasswordBtn_->setStyleSheet(
        "QPushButton {"
        "  background: #f8f9fa;"
        "  color: #6c757d;"
        "  border: 1px solid #dee2e6;"
        "  border-radius: 8px;"
        "  padding: 8px 15px;"
        "  font-size: 13px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background: #e9ecef;"
        "  color: #495057;"
        "  border-color: #adb5bd;"
        "}"
        "QPushButton:pressed {"
        "  background: #dee2e6;"
        "}"
    );
    formLayout->addWidget(showPasswordBtn_, row, 1, Qt::AlignRight);
    
    // 连接密码显示切换信号
    connect(showPasswordBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onShowPasswordToggled);
    
    // 设置列宽比例
    formLayout->setColumnStretch(0, 1);
    formLayout->setColumnStretch(1, 2);
    
    groupLayout->addLayout(formLayout);
    contentLayout->addWidget(asrConfigGroup_);
}

void SystemConfigWindow::setupAboutSection(QVBoxLayout* contentLayout) {
    aboutGroup_ = new QFrame();
    aboutGroup_->setStyleSheet(
        "QFrame {"
        "  background: white;"
        "  border: 1px solid #e1e5e9;"
        "  border-radius: 15px;"
        "  padding: 25px;"
        "}"
    );
    
    QVBoxLayout* groupLayout = new QVBoxLayout(aboutGroup_);
    groupLayout->setSpacing(20);
    
    // 标题
    QLabel* groupTitle = new QLabel("ℹ️ 关于项目");
    groupTitle->setStyleSheet(
        "QLabel {"
        "  color: #2c3e50;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  margin-bottom: 10px;"
        "}"
    );
    groupLayout->addWidget(groupTitle);
    
    // 创建关于项目组件
    aboutWidget_ = new AboutProjectWidget();
    groupLayout->addWidget(aboutWidget_);
    
    contentLayout->addWidget(aboutGroup_);
}

void SystemConfigWindow::setupBottomSection() {
    // 底部按钮容器
    QFrame* bottomFrame = new QFrame();
    bottomFrame->setStyleSheet(
        "QFrame {"
        "  background: white;"
        "  border-top: 1px solid #e1e5e9;"
        "  padding: 20px;"
        "}"
    );
    
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomFrame);
    bottomLayout->setSpacing(15);
    
    // 主要操作按钮
    buttonLayout_ = new QHBoxLayout();
    buttonLayout_->setSpacing(15);
    buttonLayout_->addStretch();

    // 保存按钮
    saveBtn_ = new QPushButton("💾 保存配置");
    saveBtn_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    saveBtn_->setMinimumHeight(45);
    saveBtn_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #28a745, stop:1 #20c997);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 10px;"
        "  padding: 12px 25px;"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #218838, stop:1 #1ea085);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #1e7e34, stop:1 #1a7a6b);"
        "}"
        "QPushButton:disabled {"
        "  background: #6c757d;"
        "  color: #adb5bd;"
        "}"
    );
    buttonLayout_->addWidget(saveBtn_);

    // 测试按钮
    testBtn_ = new QPushButton("🔍 测试连接");
    testBtn_->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    testBtn_->setMinimumHeight(45);
    testBtn_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #007bff, stop:1 #0056b3);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 10px;"
        "  padding: 12px 25px;"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #0056b3, stop:1 #004085);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #004085, stop:1 #002752);"
        "}"
        "QPushButton:disabled {"
        "  background: #6c757d;"
        "  color: #adb5bd;"
        "}"
    );
    buttonLayout_->addWidget(testBtn_);

    // 配置指南按钮
    guideBtn_ = new QPushButton("📖 配置指南");
    guideBtn_->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
    guideBtn_->setMinimumHeight(45);
    guideBtn_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #6f42c1, stop:1 #5a32a3);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 10px;"
        "  padding: 12px 25px;"
        "  font-weight: bold;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #5a32a3, stop:1 #4a2b8a);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #4a2b8a, stop:1 #3d2371);"
        "}"
        "QPushButton:disabled {"
        "  background: #6c757d;"
        "  color: #adb5bd;"
        "}"
    );
    buttonLayout_->addWidget(guideBtn_);

    buttonLayout_->addStretch();
    bottomLayout->addLayout(buttonLayout_);
    
    // 返回按钮
    QHBoxLayout* backLayout = new QHBoxLayout();
    backLayout->addStretch();

    backBtn_ = new QPushButton("← 返回主菜单");
    backBtn_->setIcon(style()->standardIcon(QStyle::SP_ArrowLeft));
    backBtn_->setMinimumHeight(40);
    backBtn_->setStyleSheet(
        "QPushButton {"
        "  background: #f8f9fa;"
        "  color: #6c757d;"
        "  border: 1px solid #dee2e6;"
        "  border-radius: 8px;"
        "  padding: 10px 20px;"
        "  font-weight: 500;"
        "  font-size: 13px;"
        "}"
        "QPushButton:hover {"
        "  background: #e9ecef;"
        "  color: #495057;"
        "  border-color: #adb5bd;"
        "}"
        "QPushButton:pressed {"
        "  background: #dee2e6;"
        "}"
    );
    backLayout->addWidget(backBtn_);
    backLayout->addStretch();
    
    bottomLayout->addLayout(backLayout);
    mainLayout_->addWidget(bottomFrame);

    // 连接信号
    connect(saveBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onSaveConfig);
    connect(testBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onTestConnection);
    connect(guideBtn_, &QPushButton::clicked, this, &SystemConfigWindow::showConfigGuide);
    connect(backBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onBackToMainMenu);
}

void SystemConfigWindow::setupStatusSection() {
    // 状态标签
    statusLabel_ = new QLabel();
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setMinimumHeight(50);
    statusLabel_->setMaximumHeight(50);
    statusLabel_->setStyleSheet(
        "QLabel {"
        "  background: #e3f2fd;"
        "  border: 1px solid #bbdefb;"
        "  border-radius: 10px;"
        "  padding: 12px 20px;"
        "  color: #1976d2;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "  margin: 0 20px 20px 20px;"
        "}"
    );
    mainLayout_->addWidget(statusLabel_);
    
    // 测试进度条
    testProgressBar_ = new QProgressBar();
    testProgressBar_->setVisible(false);
    testProgressBar_->setRange(0, 0); // 不确定进度
    testProgressBar_->setMinimumHeight(8);
    testProgressBar_->setMaximumHeight(8);
    testProgressBar_->setStyleSheet(
        "QProgressBar {"
        "  border: none;"
        "  border-radius: 4px;"
        "  background: #f1f3f4;"
        "  margin: 0 20px 20px 20px;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #FF8C00, stop:1 #FF6B35);"
        "  border-radius: 4px;"
        "}"
    );
    mainLayout_->addWidget(testProgressBar_);
}

void SystemConfigWindow::loadCurrentConfig() {
    qDebug() << "[loadCurrentConfig] called";
    // 从配置管理器加载当前配置
    AsrConfig config = configManager_->loadConfig();
    
    // 设置UI组件 - 只保留三个关键参数
    if (appIdEdit_) {
        appIdEdit_->setText(QString::fromStdString(config.appId));
    }
    if (accessTokenEdit_) {
        accessTokenEdit_->setText(QString::fromStdString(config.accessToken));
    }
    if (secretKeyEdit_) {
        secretKeyEdit_->setText(QString::fromStdString(config.secretKey));
    }
    
    configModified_ = false;
    updateConfigStatus();
}

void SystemConfigWindow::updateConfigStatus() {
    if (configModified_) {
        setWindowTitle("系统配置 *");
    } else {
        setWindowTitle("系统配置");
    }
    
    // 显示配置状态
    QString status = configManager_->getConfigStatus();
    showStatusMessage(status);
}

void SystemConfigWindow::onSaveConfig() {
    std::cout << "[UI] Save config button clicked" << std::endl;
    
    // 获取当前配置
    AsrConfig config;
    config.appId = appIdEdit_->text().toStdString();
    config.accessToken = accessTokenEdit_->text().toStdString();
    config.secretKey = secretKeyEdit_->text().toStdString();
    
    if (config.appId.empty() || config.accessToken.empty()) {
        QMessageBox::warning(this, "配置错误", "请填写App ID和Access Token");
        return;
    }
    
    std::cout << "[ASR-CRED] 开始保存配置..." << std::endl;
    std::cout << "[ASR-CRED] 保存配置:";
    std::cout << "  - App ID: " << config.appId << std::endl;
    std::cout << "  - Access Token: " << config.accessToken << std::endl;
    std::cout << "  - Secret Key: " << config.secretKey << std::endl;
    
    // 先测试连接
    std::cout << "[ASR-CRED] 保存前先测试连接..." << std::endl;
    
    // 禁用保存按钮，显示加载状态
    saveBtn_->setEnabled(false);
    saveBtn_->setText("测试中...");
    
    // 使用QtConcurrent在后台线程中执行测试
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, config]() {
        bool success = watcher->result();
        
        // 恢复按钮状态
        saveBtn_->setEnabled(true);
        saveBtn_->setText("保存配置");
        
        if (success) {
            std::cout << "[ASR-CRED] 连接测试成功，开始保存配置..." << std::endl;
            
            // 保存配置到QSettings
            QSettings settings;
            settings.setValue("asr/appId", QString::fromStdString(config.appId));
            settings.setValue("asr/accessToken", QString::fromStdString(config.accessToken));
            settings.setValue("asr/secretKey", QString::fromStdString(config.secretKey));
            settings.sync();
            
            std::cout << "[ASR-CRED] 配置保存成功" << std::endl;
            
            // 设置全局ASR有效标志
            perfx::ui::asr_valid = 1;
            
            QMessageBox::information(this, "保存成功", 
                "ASR配置保存成功！\n\n"
                "✅ 配置已验证并保存\n"
                "✅ ASR功能已启用\n\n"
                "现在可以使用ASR语音识别功能了。",
                QMessageBox::Ok);
        } else {
            std::cout << "[ASR-CRED] 连接测试失败，不保存配置" << std::endl;
            QMessageBox::critical(this, "保存失败", 
                "ASR配置保存失败！\n\n"
                "连接测试失败，请检查配置信息是否正确。\n\n"
                "请检查以下项目：\n"
                "1. App ID是否正确\n"
                "2. Access Token是否有效\n"
                "3. 网络连接是否正常\n"
                "4. 服务是否可用",
                QMessageBox::Ok);
        }
        
        watcher->deleteLater();
    });
    
    // 启动后台测试
    QFuture<bool> future = QtConcurrent::run([config]() {
        try {
            std::cout << "[ASR-THREAD] 在后台线程中执行连接测试..." << std::endl;
            
            // 创建ASR客户端进行测试
            Asr::AsrClient client;
            
            // 设置配置
            client.setAppId(config.appId);
            client.setAccessToken(config.accessToken);
            client.setSecretKey(config.secretKey);
            
            // 尝试连接（设置超时）
            bool connected = client.connect();
            
            if (connected) {
                std::cout << "[ASR-THREAD] 连接成功，断开连接..." << std::endl;
                client.disconnect();
                std::cout << "[ASR-THREAD] 连接测试完成" << std::endl;
                return true;
            } else {
                std::cout << "[ASR-THREAD] 连接失败" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ASR-THREAD][ERROR] 连接测试异常: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[ASR-THREAD][ERROR] 连接测试未知异常" << std::endl;
            return false;
        }
    });
    
    watcher->setFuture(future);
}

void SystemConfigWindow::onTestConnection() {
    std::cout << "[UI] Test connection button clicked" << std::endl;
    
    // 获取当前配置
    AsrConfig config;
    config.appId = appIdEdit_->text().toStdString();
    config.accessToken = accessTokenEdit_->text().toStdString();
    config.secretKey = secretKeyEdit_->text().toStdString();
    
    if (config.appId.empty() || config.accessToken.empty()) {
        QMessageBox::warning(this, "配置错误", "请填写App ID和Access Token");
        return;
    }
    
    std::cout << "[ASR-CRED] 开始测试连接..." << std::endl;
    std::cout << "[ASR-CRED] 测试配置:";
    std::cout << "  - App ID: " << config.appId << std::endl;
    std::cout << "  - Access Token: " << config.accessToken << std::endl;
    std::cout << "  - Secret Key: " << config.secretKey << std::endl;
    
    // 禁用测试按钮，显示加载状态
    testBtn_->setEnabled(false);
    testProgressBar_->setVisible(true);
    
    // 使用QtConcurrent在后台线程中执行测试
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool success = watcher->result();
        
        // 恢复按钮状态
        testBtn_->setEnabled(true);
        testProgressBar_->setVisible(false);
        
        if (success) {
            std::cout << "[ASR-CRED] 连接测试成功" << std::endl;
            QMessageBox::information(this, "连接成功", 
                "ASR服务连接测试成功！\n\n"
                "配置有效，可以正常使用ASR功能。",
                QMessageBox::Ok);
        } else {
            std::cout << "[ASR-CRED] 连接测试失败" << std::endl;
            QMessageBox::critical(this, "连接失败", 
                "ASR服务连接测试失败！\n\n"
                "请检查以下项目：\n"
                "1. App ID是否正确\n"
                "2. Access Token是否有效\n"
                "3. 网络连接是否正常\n"
                "4. 服务是否可用",
                QMessageBox::Ok);
        }
        
        watcher->deleteLater();
    });
    
    // 启动后台测试
    QFuture<bool> future = QtConcurrent::run([config]() {
        try {
            std::cout << "[ASR-THREAD] 在后台线程中执行连接测试..." << std::endl;
            
            // 创建ASR客户端进行测试
            Asr::AsrClient client;
            
            // 设置配置
            client.setAppId(config.appId);
            client.setAccessToken(config.accessToken);
            client.setSecretKey(config.secretKey);
            
            // 尝试连接（设置超时）
            bool connected = client.connect();
            
            if (connected) {
                std::cout << "[ASR-THREAD] 连接成功，断开连接..." << std::endl;
                client.disconnect();
                std::cout << "[ASR-THREAD] 连接测试完成" << std::endl;
                return true;
            } else {
                std::cout << "[ASR-THREAD] 连接失败" << std::endl;
                return false;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ASR-THREAD][ERROR] 连接测试异常: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "[ASR-THREAD][ERROR] 连接测试未知异常" << std::endl;
            return false;
        }
    });
    
    watcher->setFuture(future);
}

void SystemConfigWindow::onBackToMainMenu() {
    if (configModified_) {
        int ret = QMessageBox::question(this, "未保存的更改", 
                                       "配置已修改但未保存，是否保存？",
                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Yes) {
            onSaveConfig();
        } else if (ret == QMessageBox::Cancel) {
            return;
        }
    }
    
    emit backToMainMenuRequested();
}

void SystemConfigWindow::onShowPasswordToggled(bool checked) {
    QLineEdit::EchoMode mode = checked ? QLineEdit::Normal : QLineEdit::Password;
    accessTokenEdit_->setEchoMode(mode);
    secretKeyEdit_->setEchoMode(mode);
    showPasswordBtn_->setText(checked ? "🙈 隐藏密码" : "👁 显示密码");
}

void SystemConfigWindow::onConfigChanged() {
    configModified_ = true;
    updateConfigStatus();
}

void SystemConfigWindow::onConfigManagerUpdated() {
    // 配置管理器更新时，重新加载配置
    loadCurrentConfig();
}

bool SystemConfigWindow::validateConfig() {
    if (appIdEdit_->text().trimmed().isEmpty()) {
        showStatusMessage("❌ 应用ID不能为空", true);
        return false;
    }
    
    if (accessTokenEdit_->text().trimmed().isEmpty()) {
        showStatusMessage("❌ 访问令牌不能为空", true);
        return false;
    }
    
    return true;
}

bool SystemConfigWindow::testAsrConnection() {
    AsrConfig config;
    config.appId = appIdEdit_->text().toStdString();
    config.accessToken = accessTokenEdit_->text().toStdString();
    config.secretKey = secretKeyEdit_->text().toStdString();
    
    // 基本参数验证
    if (config.appId.empty()) {
        qDebug() << "[testAsrConnection] App ID is empty";
        return false;
    }
    
    if (config.accessToken.empty()) {
        qDebug() << "[testAsrConnection] Access Token is empty";
        return false;
    }
    
    if (config.secretKey.empty()) {
        qDebug() << "[testAsrConnection] Secret Key is empty";
        return false;
    }
    
    qDebug() << "[testAsrConnection] Starting ASR connection test...";
    qDebug() << "[testAsrConnection] App ID:" << QString::fromStdString(config.appId);
    qDebug() << "[testAsrConnection] Access Token:" << QString::fromStdString(config.accessToken).left(4) + "****" + QString::fromStdString(config.accessToken).right(4);
    qDebug() << "[testAsrConnection] Secret Key:" << QString::fromStdString(config.secretKey).left(4) + "****" + QString::fromStdString(config.secretKey).right(4);
    
    // 调用ASR管理器的测试接口
    bool result = Asr::AsrManager().testConnection(config.appId, config.accessToken, config.secretKey);
    
    if (result) {
        qDebug() << "[testAsrConnection] ASR connection test successful";
    } else {
        qDebug() << "[testAsrConnection] ASR connection test failed";
    }
    
    return result;
}

void SystemConfigWindow::showStatusMessage(const QString& message, bool isError) {
    statusLabel_->setText(message);
    if (isError) {
        statusLabel_->setStyleSheet(
            "QLabel {"
            "  background: #ffe6e6;"
            "  border: 1px solid #ff9999;"
            "  border-radius: 10px;"
            "  padding: 12px 20px;"
            "  color: #cc0000;"
            "  font-size: 14px;"
            "  font-weight: 500;"
            "  margin: 0 20px 20px 20px;"
            "}"
        );
    } else {
        statusLabel_->setStyleSheet(
            "QLabel {"
            "  background: #e8f4fd;"
            "  border: 1px solid #b3d9ff;"
            "  border-radius: 10px;"
            "  padding: 12px 20px;"
            "  color: #0066cc;"
            "  font-size: 14px;"
            "  font-weight: 500;"
            "  margin: 0 20px 20px 20px;"
            "}"
        );
    }
    
    // 3秒后清除消息
    statusTimer_->start(3000);
}

void SystemConfigWindow::showConfigGuide() {
    QString guide = 
        "🔧 ASR配置指南\n\n"
        "=== 配置选项说明 ===\n\n"
        "选项1：申请个人API密钥（推荐，功能完整）\n"
        "   1. 访问火山引擎官网：https://www.volcengine.com\n"
        "   2. 注册账号并创建ASR应用\n"
        "   3. 获取您的App ID、Access Token和Secret Key\n"
        "   4. 在配置界面输入这些信息\n"
        "   5. 点击保存并测试连接\n\n"
        "   💰 费用说明：\n"
        "   - 具体资费请咨询火山引擎客服\n"
        "   - 个人用户可根据使用量选择合适的套餐\n\n"
        "   ⚠️ 注意事项：\n"
        "   - 申请过程可能需要一定技术基础\n"
        "   - 建议先了解相关费用后再决定是否申请\n\n"
        "选项2：使用软件提供的体验配置（功能受限）\n"
        "   - 本软件提供基础的ASR Cloud配置\n"
        "   - 仅用于软件功能体验和测试\n"
        "   - 功能会受到一定限制\n"
        "   - 无法提供完整的免费服务\n\n"
        "   ⚠️ 重要说明：\n"
        "   - 体验配置仅供功能演示使用\n"
        "   - 我们无法长期免费提供相关服务\n"
        "   - 如需完整功能，建议申请个人API密钥\n\n"
        "=== 配置步骤 ===\n"
        "1. 选择配置方式（个人API密钥或体验配置）\n"
        "2. 在配置界面输入相应信息\n"
        "3. 点击'保存配置'按钮\n"
        "4. 点击'测试连接'验证配置是否正确\n"
        "5. 配置成功后即可使用软件功能\n\n"
        "💡 安全建议：\n"
        "- 请妥善保管您的API密钥信息\n"
        "- 不要在公共场所或不安全的环境中输入密钥\n"
        "- 定期更新API密钥以确保安全\n\n"
        "🔗 更多帮助：\n"
        "- 火山引擎官网：https://www.volcengine.com\n"
        "- 如有技术问题，请联系火山引擎客服\n"
        "- 软件使用问题，请查看软件文档或联系开发者";
    
    QMessageBox::information(this, "配置指南", guide);
}

} // namespace perfx
} // namespace ui 