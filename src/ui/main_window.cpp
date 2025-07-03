#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include "ui/main_window.h"
#include "ui/audio_to_text_window.h"
#include "ui/realtime_audio_to_text_window.h"
#include "ui/system_config_window.h"
#include "ui/app_icon_button.h"
#include "ui/config_manager.h"
#include "ui/global_state.h"
#include "asr/secure_key_manager.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QWidget>
#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QCloseEvent>
#include <QApplication>
#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QPointer>
#include <QFrame>
#include <QEvent>
#include <QVariant>
#include <QIcon>
#include <QFile>
#include <QPixmap>
#include "ui/ui_effects_manager.h"

namespace perfx {
namespace ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , stackedWidget_(nullptr)
    , audioToTextWindow_(nullptr)
    , realtimeAudioToTextWindow_(nullptr)
    , systemConfigWindow_(nullptr)
    , mainMenuWidget_(nullptr)
    , configManager_(nullptr)
    , asrConfigLoaded_(false)
    , asrConfigValid_(false)
{
    // Make the window frameless and transparent
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setupUi();
}

void MainWindow::setupUi() {
    setWindowTitle("PerfxAgent - AI音频应用");
    resize(450, 900); // 增加窗口大小以适应新设计

    // Main container with rounded corners and gradient
    QWidget* centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    centralWidget->setStyleSheet(
        "#centralWidget {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 #667eea, stop:1 #764ba2);"
        "  border-radius: 25px;"
        "  border: 2px solid rgba(255, 255, 255, 0.1);"
        "}"
    );
    
    // 添加阴影效果
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setBlurRadius(20);
    shadowEffect->setColor(QColor(0, 0, 0, 80));
    shadowEffect->setOffset(0, 5);
    centralWidget->setGraphicsEffect(shadowEffect);
    
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Custom Title Bar
    setupTitleBar(mainLayout);
    
    // Create stacked widget for different pages
    stackedWidget_ = new QStackedWidget(this);
    stackedWidget_->setStyleSheet(
        "QStackedWidget {"
        "  background: transparent;"
        "  border: none;"
        "}"
    );
    mainLayout->addWidget(stackedWidget_);
    
    // Create main menu page
    createMainMenuPage();
    stackedWidget_->addWidget(mainMenuWidget_);
    
    // Create system configuration window (this needs to be pre-created because configuration might affect other features)
    systemConfigWindow_ = new SystemConfigWindow(this);
    stackedWidget_->addWidget(systemConfigWindow_);
    
    // Create audio to text window
    audioToTextWindow_ = new AudioToTextWindow(this);
    stackedWidget_->addWidget(audioToTextWindow_);
    
    // Connect system configuration window signals
    connect(systemConfigWindow_, &SystemConfigWindow::backToMainMenuRequested,
            this, &MainWindow::switchToMainMenu);
    connect(systemConfigWindow_, &SystemConfigWindow::configUpdated,
            this, &MainWindow::onConfigUpdated);
    
    // Connect audio to text window signals
    connect(audioToTextWindow_, &AudioToTextWindow::backToMainMenuRequested,
            this, &MainWindow::switchToMainMenu);

    // Start with main menu
    stackedWidget_->setCurrentWidget(mainMenuWidget_);

    // 主菜单显示后，2秒后再初始化ASR配置
    QTimer::singleShot(2000, this, &MainWindow::loadAsrConfig);
}

void MainWindow::setupTitleBar(QVBoxLayout* mainLayout) {
    QFrame* titleBar = new QFrame();
    titleBar->setFixedHeight(60);
    titleBar->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 rgba(255, 255, 255, 0.1), stop:1 rgba(255, 255, 255, 0.05));"
        "  border-top-left-radius: 25px;"
        "  border-top-right-radius: 25px;"
        "  border-bottom: 1px solid rgba(255, 255, 255, 0.1);"
        "}"
    );
    
    QHBoxLayout* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 10, 20, 10);
    titleLayout->setSpacing(15);
    
    // 应用标题
    QLabel* titleLabel = new QLabel("PerfxAgent");
    titleLabel->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 18px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "}"
    );
    titleLayout->addWidget(titleLabel);
    
    titleLayout->addStretch();
    
    // 窗口控制按钮
    QPushButton* minimizeButton = new QPushButton("─");
    QPushButton* closeButton = new QPushButton("✕");
    
    minimizeButton->setFixedSize(30, 30);
    closeButton->setFixedSize(30, 30);
    
    minimizeButton->setStyleSheet(
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.1);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 15px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 255, 255, 0.2);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(255, 255, 255, 0.3);"
        "}"
    );
    
    closeButton->setStyleSheet(
        "QPushButton {"
        "  background: rgba(255, 255, 255, 0.1);"
        "  color: white;"
        "  border: none;"
        "  border-radius: 15px;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255, 107, 107, 0.8);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(255, 107, 107, 1.0);"
        "}"
    );

    titleLayout->addWidget(minimizeButton);
    titleLayout->addWidget(closeButton);

    mainLayout->addWidget(titleBar);

    connect(closeButton, &QPushButton::clicked, this, &MainWindow::close);
    connect(minimizeButton, &QPushButton::clicked, this, &MainWindow::showMinimized);
}

void MainWindow::loadAsrConfig() {
    try {
        qDebug() << "==========================================";
        qDebug() << "[loadAsrConfig] 🚀 开始加载ASR配置...";
        qDebug() << "==========================================";
        
        // 获取配置管理器实例
        configManager_ = ConfigManager::instance();
        qDebug() << "[loadAsrConfig] ✅ 配置管理器实例获取成功";
        
        // 使用QPointer来安全地检查对象是否还存在
        QPointer<MainWindow> self(this);
        
        // 延迟一点点后验证ASR
        QTimer::singleShot(100, this, [self]() {
            try {
                // 检查MainWindow是否还存在
                if (!self) {
                    qDebug() << "[loadAsrConfig] ❌ MainWindow已被销毁，跳过ASR验证";
                    return;
                }
                
                qDebug() << "[loadAsrConfig] 🔍 开始验证ASR配置...";
                
                // 检查配置管理器是否有效
                if (!self->configManager_) {
                    qDebug() << "[loadAsrConfig] ❌ 配置管理器无效，跳过ASR验证";
                    perfx::ui::asr_valid = 0;
                    return;
                }
                
                qDebug() << "[loadAsrConfig] 📋 开始加载ASR配置...";
                
                // 加载ASR配置
                AsrConfig config = self->configManager_->loadConfig();
                self->asrConfigLoaded_ = true;
                self->asrConfigValid_ = self->configManager_->hasValidConfig();
                
                // 详细打印配置信息
                qDebug() << "[loadAsrConfig] 📊 配置加载结果:";
                qDebug() << "   - 配置来源: " << QString::fromStdString(config.configSource);
                qDebug() << "   - App ID: " << QString::fromStdString(config.appId);
                qDebug() << "   - Access Token: " << QString::fromStdString(Asr::maskSensitiveInfo(config.accessToken, 4, 4));
                qDebug() << "   - Secret Key: " << QString::fromStdString(Asr::maskSensitiveInfo(config.secretKey, 4, 4));
                qDebug() << "   - 配置有效: " << (config.isValid ? "是" : "否");
                qDebug() << "   - 配置已加载: " << (self->asrConfigLoaded_ ? "是" : "否");
                qDebug() << "   - 配置验证: " << (self->asrConfigValid_ ? "通过" : "失败");
                
                // 打印配置来源说明
                if (config.configSource == "environment_variables") {
                    qDebug() << "[loadAsrConfig] 🎯 使用环境变量配置 (ASR_* 前缀)";
                    qDebug() << "   💡 这是用户自定义的环境变量配置，优先级最高";
                } else if (config.configSource == "user_config") {
                    qDebug() << "[loadAsrConfig] 🎯 使用用户界面配置";
                    qDebug() << "   💡 这是通过系统配置界面保存的配置";
                } else if (config.configSource == "trial_mode") {
                    qDebug() << "[loadAsrConfig] 🎯 使用体验模式配置";
                    qDebug() << "   💡 这是厂商提供的混淆配置，用于体验功能";
                    qDebug() << "   💡 建议设置环境变量以获得完整功能";
                } else {
                    qDebug() << "[loadAsrConfig] 🎯 使用未知配置来源: " << QString::fromStdString(config.configSource);
                }
                
                if (self->asrConfigValid_) {
                    perfx::ui::asr_valid = 1;
                    qDebug() << "[loadAsrConfig] ✅ ASR验证成功 - 凭证有效";
                    qDebug() << "   🎉 现在可以使用ASR功能了！";
                } else {
                    perfx::ui::asr_valid = 0;
                    qDebug() << "[loadAsrConfig] ❌ ASR验证失败 - 需要用户配置";
                    qDebug() << "   💡 请前往系统配置界面设置ASR凭证";
                }
                
                qDebug() << "==========================================";
                qDebug() << "[loadAsrConfig] 🏁 ASR配置加载完成";
                qDebug() << "==========================================";
                
            } catch (const std::exception& e) {
                qDebug() << "[loadAsrConfig] ❌ ASR验证异常:" << e.what();
                perfx::ui::asr_valid = 0;
            } catch (...) {
                qDebug() << "[loadAsrConfig] ❌ ASR验证未知异常";
                perfx::ui::asr_valid = 0;
            }
        });
    } catch (const std::exception& e) {
        qDebug() << "[loadAsrConfig] ❌ 配置管理器初始化异常:" << e.what();
        perfx::ui::asr_valid = 0;
    } catch (...) {
        qDebug() << "[loadAsrConfig] ❌ 配置管理器初始化未知异常";
        perfx::ui::asr_valid = 0;
    }
}

void MainWindow::createMainMenuPage() {
    if (mainMenuWidget_) {
        return;
    }
    
    mainMenuWidget_ = new QWidget(this);
    mainMenuWidget_->setStyleSheet(
        "QWidget {"
        "  background: transparent;"
        "}"
    );
    
    QVBoxLayout* mainMenuLayout = new QVBoxLayout(mainMenuWidget_);
    mainMenuLayout->setContentsMargins(30, 30, 30, 30);
    mainMenuLayout->setSpacing(30);

    // 欢迎标题
    QLabel* welcomeLabel = new QLabel("欢迎使用 PerfxAgent");
    welcomeLabel->setStyleSheet(
        "QLabel {"
        "  color: white;"
        "  font-size: 24px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  text-align: center;"
        "  margin-bottom: 10px;"
        "}"
    );
    welcomeLabel->setAlignment(Qt::AlignCenter);
    mainMenuLayout->addWidget(welcomeLabel);
    
    // 副标题
    QLabel* subtitleLabel = new QLabel("AI驱动的音频转文字应用");
    subtitleLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.8);"
        "  font-size: 14px;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  text-align: center;"
        "  margin-bottom: 20px;"
        "}"
    );
    subtitleLabel->setAlignment(Qt::AlignCenter);
    mainMenuLayout->addWidget(subtitleLabel);

    // 横向排列的三个自定义图标按钮
    QHBoxLayout* iconLayout = new QHBoxLayout();
    iconLayout->setSpacing(48);
    iconLayout->setContentsMargins(0, 0, 0, 0);

    // 文件转文字按钮
    perfx::ui::AnimatedButton* fileBtn = new perfx::ui::AnimatedButton(mainMenuWidget_);
    fileBtn->setIcon(QIcon(":/icons/audio_file.png"));
    fileBtn->setIconSize(QSize(72, 72));
    fileBtn->setFixedSize(120, 120);
    fileBtn->setObjectName("fileBtn");
    fileBtn->setStyleSheet(
        "#fileBtn {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.15),"
        "    stop:0.7 rgba(255, 255, 255, 0.05),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 20px;"
        "  color: white;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "#fileBtn:hover {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.25),"
        "    stop:0.7 rgba(255, 255, 255, 0.1),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.4);"
        "}"
        "#fileBtn:pressed {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.3),"
        "    stop:0.7 rgba(255, 255, 255, 0.15),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.6);"
        "}"
    );
    QGraphicsDropShadowEffect* fileShadow = new QGraphicsDropShadowEffect();
    fileShadow->setBlurRadius(15);
    fileShadow->setColor(QColor(0, 0, 0, 60));
    fileShadow->setOffset(0, 4);
    fileBtn->setGraphicsEffect(fileShadow);
    connect(fileBtn, &QPushButton::clicked, this, &MainWindow::switchToAudioToText);
    iconLayout->addWidget(fileBtn, 0, Qt::AlignHCenter);

    // 实时录音按钮
    perfx::ui::AnimatedButton* micBtn = new perfx::ui::AnimatedButton(mainMenuWidget_);
    micBtn->setIcon(QIcon(":/icons/realtime_ai_audio.png"));
    micBtn->setIconSize(QSize(72, 72));
    micBtn->setFixedSize(120, 120);
    micBtn->setObjectName("micBtn");
    micBtn->setStyleSheet(
        "#micBtn {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.15),"
        "    stop:0.7 rgba(255, 255, 255, 0.05),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 20px;"
        "  color: white;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "#micBtn:hover {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.25),"
        "    stop:0.7 rgba(255, 255, 255, 0.1),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.4);"
        "}"
        "#micBtn:pressed {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.3),"
        "    stop:0.7 rgba(255, 255, 255, 0.15),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.6);"
        "}"
    );
    QGraphicsDropShadowEffect* micShadow = new QGraphicsDropShadowEffect();
    micShadow->setBlurRadius(15);
    micShadow->setColor(QColor(0, 0, 0, 60));
    micShadow->setOffset(0, 4);
    micBtn->setGraphicsEffect(micShadow);
    connect(micBtn, &QPushButton::clicked, this, &MainWindow::switchToRealtimeAudioToText);
    iconLayout->addWidget(micBtn, 0, Qt::AlignHCenter);

    // 设置按钮
    perfx::ui::AnimatedButton* settingsBtn = new perfx::ui::AnimatedButton(mainMenuWidget_);
    settingsBtn->setIcon(QIcon(":/icons/settging_asr.png"));
    settingsBtn->setIconSize(QSize(72, 72));
    settingsBtn->setFixedSize(120, 120);
    settingsBtn->setObjectName("settingsBtn");
    settingsBtn->setStyleSheet(
        "#settingsBtn {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.15),"
        "    stop:0.7 rgba(255, 255, 255, 0.05),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.2);"
        "  border-radius: 20px;"
        "  color: white;"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "}"
        "#settingsBtn:hover {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.25),"
        "    stop:0.7 rgba(255, 255, 255, 0.1),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.4);"
        "}"
        "#settingsBtn:pressed {"
        "  background: qradialgradient(cx:0.5, cy:0.5, radius:0.8,"
        "    stop:0 rgba(255, 255, 255, 0.3),"
        "    stop:0.7 rgba(255, 255, 255, 0.15),"
        "    stop:1 rgba(255, 255, 255, 0));"
        "  border: 2px solid rgba(255, 255, 255, 0.6);"
        "}"
    );
    QGraphicsDropShadowEffect* settingsShadow = new QGraphicsDropShadowEffect();
    settingsShadow->setBlurRadius(15);
    settingsShadow->setColor(QColor(0, 0, 0, 60));
    settingsShadow->setOffset(0, 4);
    settingsBtn->setGraphicsEffect(settingsShadow);
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::switchToSystemConfig);
    iconLayout->addWidget(settingsBtn, 0, Qt::AlignHCenter);

    // 添加按钮标签
    QHBoxLayout* labelLayout = new QHBoxLayout();
    labelLayout->setSpacing(48);
    labelLayout->setContentsMargins(0, 10, 0, 0);
    
    QLabel* fileLabel = new QLabel("文件转文字");
    fileLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.9);"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  text-align: center;"
        "}"
    );
    fileLabel->setAlignment(Qt::AlignCenter);
    fileLabel->setFixedWidth(120);
    
    QLabel* micLabel = new QLabel("实时录音");
    micLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.9);"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  text-align: center;"
        "}"
    );
    micLabel->setAlignment(Qt::AlignCenter);
    micLabel->setFixedWidth(120);
    
    QLabel* settingsLabel = new QLabel("系统设置");
    settingsLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.9);"
        "  font-size: 12px;"
        "  font-weight: bold;"
        "  font-family: 'Segoe UI', 'Microsoft YaHei', Arial, sans-serif;"
        "  text-align: center;"
        "}"
    );
    settingsLabel->setAlignment(Qt::AlignCenter);
    settingsLabel->setFixedWidth(120);
    
    labelLayout->addWidget(fileLabel, 0, Qt::AlignHCenter);
    labelLayout->addWidget(micLabel, 0, Qt::AlignHCenter);
    labelLayout->addWidget(settingsLabel, 0, Qt::AlignHCenter);

    mainMenuLayout->addLayout(iconLayout);
    mainMenuLayout->addLayout(labelLayout);
    mainMenuLayout->addStretch();
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragPosition_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition_);
        event->accept();
    }
}

void MainWindow::switchToAudioToText() {
    // 检查ASR凭证有效性
    if (perfx::ui::asr_valid == 0) {
        QMessageBox::warning(this, "ASR未就绪", 
            "ASR配置未验证或验证失败，请先配置ASR。");
        return;
    }
    if (audioToTextWindow_) {
        stackedWidget_->setCurrentWidget(audioToTextWindow_);
    } else {
        qWarning() << "audioToTextWindow_ is nullptr!";
    }
}

void MainWindow::switchToRealtimeAudioToText() {
    // 检查ASR凭证有效性
    if (perfx::ui::asr_valid == 0) {
        QMessageBox::warning(this, "ASR未就绪", 
            "ASR配置未验证或验证失败，请先配置ASR。");
        return;
    }
    
    if (!realtimeAudioToTextWindow_) {
        realtimeAudioToTextWindow_ = new RealtimeAudioToTextWindow(this);
        stackedWidget_->addWidget(realtimeAudioToTextWindow_);
        connect(realtimeAudioToTextWindow_, &RealtimeAudioToTextWindow::backToMainMenuRequested, 
                this, &MainWindow::switchToMainMenu);
    }
    
    if (realtimeAudioToTextWindow_) {
        stackedWidget_->setCurrentWidget(realtimeAudioToTextWindow_);
        realtimeAudioToTextWindow_->startMicCollection();
    } else {
        qWarning() << "realtimeAudioToTextWindow_ is nullptr!";
    }
}

void MainWindow::switchToSystemConfig() {
    if (systemConfigWindow_) {
        stackedWidget_->setCurrentWidget(systemConfigWindow_);
    } else {
        qWarning() << "systemConfigWindow_ is nullptr!";
    }
}

void MainWindow::switchToMainMenu() {
    if (mainMenuWidget_) {
        stackedWidget_->setCurrentWidget(mainMenuWidget_);
    } else {
        qWarning() << "mainMenuWidget_ is nullptr!";
    }
}

void MainWindow::onConfigUpdated() {
    // 配置更新时，重新检查ASR配置状态
    if (configManager_) {
        asrConfigValid_ = configManager_->hasValidConfig();
        
        if (asrConfigValid_) {
            // 配置有效，可以显示成功消息
            QMessageBox::information(this, "配置更新", 
                "ASR配置已更新并验证成功。\n"
                "现在可以使用ASR功能了。",
                QMessageBox::Ok);
        }
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // 关闭实时转录界面（如果打开）
    if (realtimeAudioToTextWindow_) {
        realtimeAudioToTextWindow_->stopMicCollection();
    }
    
    // 重置全局状态
    perfx::ui::asr_valid = 0;
    perfx::ui::mic_valid = 0;
    
    QApplication::quit();
    event->accept();
}

MainWindow::~MainWindow() = default;

} // namespace ui
} // namespace perfx 