#include "ui/system_config_window.h"
#include "ui/about_project_widget.h"
#include "ui/config_manager.h"
#include "ui/input_method_manager.h"
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
#include "asr/secure_key_manager.h"
#include "ui/global_state.h"
#include "ui/ui_effects_manager.h"
#include <iostream>

namespace perfx {
namespace ui {

// 工具函数：中间隐码，仅显示前后各8位
static QString maskMiddle(const QString& str) {
    if (str.length() <= 16) return str;
    return str.left(8) + QString(str.length() - 16, QChar('*')) + str.right(8);
}

SystemConfigWindow::SystemConfigWindow(QWidget* parent)
    : QWidget(parent) // 继承自QWidget，parent是父窗口指针
    , mainLayout_(nullptr) // 主布局管理器，用于垂直排列所有UI元素
    , scrollArea_(nullptr) // 滚动区域，当内容超出窗口大小时可以滚动
    , contentWidget_(nullptr) // 内容容器，承载所有配置卡片
    , asrConfigGroup_(nullptr) // ASR配置卡片，包含API密钥输入框
    , aboutGroup_(nullptr) // 关于项目卡片，显示项目信息
    , appIdEdit_(nullptr) // 应用ID输入框
    , accessTokenEdit_(nullptr) // 访问令牌输入框
    , secretKeyEdit_(nullptr) // 密钥输入框
    , showPasswordBtn_(nullptr) // 显示/隐藏密码按钮
    , aboutWidget_(nullptr) // 关于项目组件
    , saveBtn_(nullptr) // 保存配置按钮
    , testBtn_(nullptr) // 测试连接按钮
    , guideBtn_(nullptr) // 配置指南按钮
    , backBtn_(nullptr) // 返回主菜单按钮
    , configManager_(nullptr) // 配置管理器实例
    , configModified_(false) // 配置是否被修改的标志
    , buttonLayout_(nullptr) // 按钮布局管理器
    , passwordVisible_(false) // 新增：密码可见性标志
{
    // 使用输入法管理器优化窗口，改善中文输入体验
    InputMethodManager::instance()->optimizeWindow(this);
    
    // 获取配置管理器实例，用于管理ASR配置
    configManager_ = ConfigManager::instance();
    
    // 设置用户界面，创建所有UI组件
    setupUI();
    
    // 优化所有输入控件，确保输入法正常工作
    InputMethodManager::instance()->optimizeAllInputWidgets(this);
    
    // 加载当前配置到UI界面
    loadCurrentConfig();
    
    // 连接配置管理器信号，当配置更新时重新加载
    connect(configManager_, &ConfigManager::configUpdated, 
            this, &SystemConfigWindow::onConfigManagerUpdated);
}

SystemConfigWindow::~SystemConfigWindow() = default; // 析构函数，Qt会自动清理UI组件

void SystemConfigWindow::setupUI() {
    // 设置窗口标题，显示在窗口标题栏
    setWindowTitle("系统配置");
    // 设置窗口初始大小为 560x900 像素，允许用户自由缩放窗口
    // 第一个参数是宽度（像素），第二个参数是高度（像素）
    // 你可以根据实际需求调整这两个数值，比如更宽的内容区可以设为 700 或 800
    // 注意：窗口实际显示宽度不会小于 setMinimumWidth 的值
    resize(560, 900); // 初始大小，允许缩放
    setStyleSheet("background: #f5f6fa;"); // 设置主窗口背景色为浅灰色，提升整体明亮感

    // 主布局，垂直排列所有区域
    // QVBoxLayout是垂直布局管理器，所有子组件会垂直排列
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setSpacing(0); // 主布局内各区域间距为0，所有间距通过 addSpacing 控制
    mainLayout_->setContentsMargins(0, 0, 0, 0); // 主布局四周无额外边距

    // 顶部留白 32 像素，让内容不紧贴窗口上边缘
    mainLayout_->addSpacing(32);

    // 创建滚动区域，内容超出时可滚动
    // QScrollArea是Qt的滚动容器，当内容超出可视区域时显示滚动条
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true); // 内容自适应滚动区大小
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); // 禁用横向滚动条
    scrollArea_->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);   // 需要时显示纵向滚动条
    // 设置滚动区域样式，包括背景色、滚动条样式等
    scrollArea_->setStyleSheet(
        "QScrollArea { background: #f5f6fa; border: none; }" // 滚动区背景色、无边框
        "QScrollBar:vertical {" // 垂直滚动条样式
        "  width: 8px;" // 滚动条宽度
        "  background: transparent;" // 滚动条背景透明
        "  margin: 0px 0px 0px 0px;" // 滚动条边距
        "  border-radius: 4px;" // 滚动条圆角
        "}"
        "QScrollBar::handle:vertical {" // 滚动条滑块样式
        "  background: #bdbdbd;" // 滚动条滑块颜色
        "  min-height: 20px;" // 滑块最小高度
        "  border-radius: 4px;" // 滑块圆角
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {" // 滚动条上下按钮
        "  height: 0px;" // 隐藏上下按钮
        "}"
    );

    // 内容容器，承载所有卡片和表单
    // QWidget是Qt的基础窗口组件，这里用作内容容器
    contentWidget_ = new QWidget();
    contentWidget_->setMaximumWidth(560); // 内容区最大宽度，防止内容过宽影响美观。可调大如700
    contentWidget_->setMinimumWidth(200); // 内容区最小宽度，防止窗口缩小时内容被压得太窄

    // 创建主内容区的垂直布局，所有控件会垂直排列
    QVBoxLayout* contentLayout = new QVBoxLayout(contentWidget_);
    contentLayout->setSpacing(16);
    contentLayout->setContentsMargins(24, 24, 24, 24);
    contentLayout->addSpacing(8);
    contentLayout->setAlignment(Qt::AlignTop); // 新增：内容靠上对齐
    setupHeaderSection(contentLayout); // 设置标题区域
    contentLayout->addSpacing(16); // 标题与ASR卡片间距
    setupAsrConfigSection(contentLayout); // 设置ASR配置区域
    contentLayout->addSpacing(16); // ASR卡片与关于卡片间距
    setupAboutSection(contentLayout); // 设置关于项目区域
    contentLayout->addSpacing(32); // 内容区底部留白

    // 居中内容卡片
    // 创建一个容器来居中显示内容
    QWidget* centerContainer = new QWidget();
    QHBoxLayout* hCenterLayout = new QHBoxLayout(centerContainer); // 水平布局用于居中
    hCenterLayout->addStretch(1); // 左侧弹性空间，推动内容向右
    hCenterLayout->addWidget(contentWidget_, 0, Qt::AlignCenter); // 内容区居中
    hCenterLayout->addStretch(1); // 右侧弹性空间，推动内容向左
    hCenterLayout->setContentsMargins(0, 0, 0, 0); // 无额外边距
    scrollArea_->setWidget(centerContainer); // 将居中容器设置为滚动区域的内容

    mainLayout_->addWidget(scrollArea_, 10); // 滚动区权重为10，主内容区
    mainLayout_->addSpacing(32); // 内容与按钮区间距

    // 底部按钮区域悬浮居中
    setupBottomSection(); // 设置底部按钮区域
    if (mainLayout_->count() > 0) {
        QWidget* bottomFrame = qobject_cast<QWidget*>(mainLayout_->itemAt(mainLayout_->count() - 1)->widget());
        if (bottomFrame) {
            bottomFrame->setMaximumWidth(560); // 按钮区最大宽度，建议与内容区一致
            bottomFrame->setMinimumWidth(320); // 按钮区最小宽度
            bottomFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed); // 宽度自适应，高度固定
            mainLayout_->setAlignment(bottomFrame, Qt::AlignHCenter); // 按钮区居中
        }
    }
    mainLayout_->addSpacing(32); // 底部留白
}

void SystemConfigWindow::setupHeaderSection(QVBoxLayout* contentLayout) {
    // 标题区域 - 创建顶部的渐变标题卡片
    QFrame* headerFrame = new QFrame(); // QFrame是Qt的框架组件，用于创建带样式的容器
    // 设置标题卡片的样式，包括渐变背景、圆角、阴影等
    headerFrame->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF8C00, stop:1 #FF6B35);" // 橙色到红色的渐变背景
        "  border-radius: 14px;" // 圆角半径，让卡片看起来更现代
        "  padding: 8px;" // 内边距，给内容留出空间
        "}"
    );
    QVBoxLayout* headerLayout = new QVBoxLayout(headerFrame); // 垂直布局，标题和副标题垂直排列
    headerLayout->setContentsMargins(12, 12, 12, 12); // 设置布局内边距，让文字不贴边
    QLabel* titleLabel = new QLabel("系统配置"); // 主标题标签
    // 设置主标题样式，白色文字，大字体，粗体
    titleLabel->setStyleSheet(
        "QLabel {"
        "  color: white;" // 文字颜色为白色
        "  font-size: 32px;" // 字体大小32像素
        "  font-weight: 900;" // 字体粗细，900是最粗的
        "  margin-bottom: 8px;" // 底部边距
        "  letter-spacing: 1px;" // 字母间距，增加可读性
        "}"
    );
    titleLabel->setAlignment(Qt::AlignLeft); // 文字左对齐
    QLabel* subtitleLabel = new QLabel("配置ASR服务和查看项目信息"); // 副标题标签
    // 设置副标题样式，半透明白色，较小字体
    subtitleLabel->setStyleSheet(
        "QLabel {"
        "  color: rgba(255, 255, 255, 0.85);" // 半透明白色，85%透明度
        "  font-size: 16px;" // 字体大小16像素
        "  font-weight: 400;" // 字体粗细，400是正常粗细
        "}"
    );
    subtitleLabel->setAlignment(Qt::AlignLeft); // 文字左对齐
    headerLayout->addWidget(titleLabel); // 将主标题添加到布局中
    headerLayout->addWidget(subtitleLabel); // 将副标题添加到布局中
    contentLayout->addWidget(headerFrame); // 将整个标题卡片添加到主内容布局中
    
    // 应用阴影效果替代CSS box-shadow
    ShadowEffect::applyShadow(headerFrame, 32, QColor(0, 0, 0, 25), QPoint(0, 6));
}

void SystemConfigWindow::setupAsrConfigSection(QVBoxLayout* contentLayout) {
    // 创建ASR配置卡片区域 - 这是主要的配置表单区域
    asrConfigGroup_ = new QFrame(); // 创建框架容器，用于承载所有ASR配置控件
    // 设置卡片样式，包括背景色、圆角、阴影等，让卡片看起来更现代
    asrConfigGroup_->setStyleSheet(
        "QFrame {"
        "  background: white;" // 卡片背景色为白色
        "  border: none;"      // 无边框，让卡片更简洁
        "  border-radius: 20px;" // 卡片圆角半径，让边角更圆润
        "  padding: 32px;"     // 卡片内部留白，让内容不贴边
        "}"
    );
    QVBoxLayout* groupLayout = new QVBoxLayout(asrConfigGroup_); // 垂直布局，所有控件垂直排列
    groupLayout->setSpacing(14); // 卡片内各分组间垂直间距，控制各元素间的距离

    // 标题 - 配置区域的标题
    QLabel* groupTitle = new QLabel("🔧 火山ASR服务配置"); // 带图标的标题
    // 设置标题样式，深色文字，大字体，粗体
    groupTitle->setStyleSheet(
        "QLabel {"
        "  color: #222;" // 深灰色文字
        "  font-size: 22px;" // 标题字号22像素
        "  font-weight: 800;" // 字体粗细，800是较粗的
        "  margin-bottom: 10px;" // 底部边距
        "}"
    );
    groupLayout->addWidget(groupTitle); // 将标题添加到布局中

    // 配置说明 - 帮助用户理解配置的说明文字
    QLabel* descriptionLabel = new QLabel(
        "配置火山引擎ASR服务，支持语音转文字功能。请确保您已申请相关API密钥。"
    );
    // 设置说明文字样式，浅色背景，带左边框
    descriptionLabel->setStyleSheet(
        "QLabel {"
        "  color: #888;" // 灰色文字
        "  font-size: 15px;" // 字体大小15像素
        "  line-height: 1.5;" // 行高，让文字更易读
        "  padding: 12px;" // 说明文字的内边距
        "  background: #f7f8fa;" // 浅灰色背景
        "  border-radius: 10px;" // 圆角
        "  border-left: 5px solid #FF8C00;" // 左边框，橙色
        "}"
    );
    descriptionLabel->setWordWrap(true); // 允许文字换行
    groupLayout->addWidget(descriptionLabel); // 将说明添加到布局中

    // 配置表单 - 改为每个标签和输入框上下排列，整体更紧凑
    QVBoxLayout* formLayout = new QVBoxLayout();
    formLayout->setSpacing(8); // 控件间距更紧凑
    formLayout->setContentsMargins(0, 0, 0, 0);

    // App ID
    QLabel* appIdLabel = new QLabel("App ID");
    appIdLabel->setStyleSheet(
        "QLabel { color: #333; font-weight: 700; font-size: 15px; margin-bottom: 2px; }"
    );
    formLayout->addWidget(appIdLabel);
    appIdEdit_ = new QLineEdit();
    appIdEdit_->setPlaceholderText("8388341111");
    appIdEdit_->setMinimumHeight(38);
    appIdEdit_->setStyleSheet(
        "QLineEdit { border: 1.5px solid #e0e0e0; border-radius: 14px; padding: 10px 16px; background: #fafbfc; font-size: 15px; color: #222; }"
        "QLineEdit:focus { border-color: #FF8C00; background: #fff8f0; }"
        "QLineEdit::placeholder { color: #bbb; font-style: italic; }"
    );
    appIdEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addWidget(appIdEdit_);

    // Access Token
    QLabel* accessTokenLabel = new QLabel("Access Token");
    accessTokenLabel->setStyleSheet(
        "QLabel { color: #333; font-weight: 700; font-size: 15px; margin-top: 8px; margin-bottom: 2px; }"
    );
    formLayout->addWidget(accessTokenLabel);
    accessTokenEdit_ = new QLineEdit();
    accessTokenEdit_->setPlaceholderText("请vQWuOVrgH6J0kCAQo****_****5q2lG3");
    accessTokenEdit_->setMinimumHeight(38);
    accessTokenEdit_->setStyleSheet(
        "QLineEdit { border: 1.5px solid #e0e0e0; border-radius: 14px; padding: 10px 16px; background: #fafbfc; font-size: 15px; color: #222; }"
        "QLineEdit:focus { border-color: #FF8C00; background: #fff8f0; }"
        "QLineEdit::placeholder { color: #bbb; font-style: italic; }"
    );
    accessTokenEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addWidget(accessTokenEdit_);

    // Secret Key
    QLabel* secretKeyLabel = new QLabel("Secret Key");
    secretKeyLabel->setStyleSheet(
        "QLabel { color: #333; font-weight: 700; font-size: 15px; margin-top: 8px; margin-bottom: 2px; }"
    );
    formLayout->addWidget(secretKeyLabel);
    secretKeyEdit_ = new QLineEdit();
    secretKeyEdit_->setPlaceholderText("oKzfTdLm0M2dVUXUKW86jb-*******3e");
    secretKeyEdit_->setMinimumHeight(38);
    secretKeyEdit_->setStyleSheet(
        "QLineEdit { border: 1.5px solid #e0e0e0; border-radius: 14px; padding: 10px 16px; background: #fafbfc; font-size: 15px; color: #222; }"
        "QLineEdit:focus { border-color: #FF8C00; background: #fff8f0; }"
        "QLineEdit::placeholder { color: #bbb; font-style: italic; }"
    );
    secretKeyEdit_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    formLayout->addWidget(secretKeyEdit_);

    // 显示/隐藏密码按钮
    showPasswordBtn_ = new QPushButton("👁 显示密码");
    showPasswordBtn_->setMaximumWidth(160);
    showPasswordBtn_->setMinimumHeight(36);
    showPasswordBtn_->setStyleSheet(
        "QPushButton { background: #f5f6fa; color: #666; border: 1.5px solid #e0e0e0; border-radius: 10px; padding: 8px 18px; font-size: 14px; font-weight: 500; }"
        "QPushButton:hover { background: #e0e0e0; color: #222; border-color: #bbb; }"
        "QPushButton:pressed { background: #e0e0e0; }"
    );
    formLayout->addWidget(showPasswordBtn_, 0, Qt::AlignRight);
    connect(showPasswordBtn_, &QPushButton::clicked, this, [this]() {
        passwordVisible_ = !passwordVisible_;
        QLineEdit::EchoMode mode = passwordVisible_ ? QLineEdit::Normal : QLineEdit::Password;
        accessTokenEdit_->setEchoMode(mode);
        secretKeyEdit_->setEchoMode(mode);
        AsrConfig config = configManager_->loadConfig();
        if (passwordVisible_) {
            accessTokenEdit_->setText(QString::fromStdString(config.accessToken));
            secretKeyEdit_->setText(QString::fromStdString(config.secretKey));
            showPasswordBtn_->setText("🙈 隐藏密码");
        } else {
            accessTokenEdit_->setText(maskMiddle(QString::fromStdString(config.accessToken)));
            secretKeyEdit_->setText(maskMiddle(QString::fromStdString(config.secretKey)));
            showPasswordBtn_->setText("👁 显示密码");
        }
    });

    groupLayout->addLayout(formLayout);
    contentLayout->addWidget(asrConfigGroup_); // 将整个ASR配置组添加到主内容布局中
    
    // 应用阴影效果替代CSS box-shadow
    ShadowEffect::applyShadow(asrConfigGroup_, 24, QColor(0, 0, 0, 20), QPoint(0, 4));
}

void SystemConfigWindow::setupAboutSection(QVBoxLayout* contentLayout) {
    // 关于项目区域 - 创建显示项目信息的卡片
    aboutGroup_ = new QFrame(); // 创建框架容器，用于承载关于项目的内容
    // 设置关于卡片的样式，与ASR配置卡片相同
    aboutGroup_->setStyleSheet(
        "QFrame {"
        "  background: #f5f6fa;"
        "  color: #222;"
        "  border: none;"
        "  border-radius: 20px;"
        "  padding: 12px;"
        "}"
    );
    QVBoxLayout* groupLayout = new QVBoxLayout(aboutGroup_); // 垂直布局，所有内容垂直排列
    groupLayout->setSpacing(24); // 各元素间的垂直间距24像素
    
    // 关于项目的标题
    QLabel* groupTitle = new QLabel("ℹ️ 关于项目"); // 带信息图标的标题
    // 设置标题样式，与ASR配置标题相同
    groupTitle->setStyleSheet(
        "QLabel {"
        "  color: #222;" // 深灰色文字
        "  font-size: 22px;" // 字体大小22像素
        "  font-weight: 800;" // 字体粗细，800是较粗的
        "  margin-bottom: 10px;" // 底部边距10像素
        "}"
    );
    groupLayout->addWidget(groupTitle); // 将标题添加到布局中
    
    // 创建关于项目组件，这是一个自定义组件，显示项目的详细信息
    aboutWidget_ = new AboutProjectWidget(); // 创建关于项目组件实例
    groupLayout->addWidget(aboutWidget_); // 将关于项目组件添加到布局中
    
    contentLayout->addWidget(aboutGroup_); // 将整个关于组添加到主内容布局中
    
    // 应用阴影效果替代CSS box-shadow
    ShadowEffect::applyShadow(aboutGroup_, 24, QColor(0, 0, 0, 20), QPoint(0, 4));
}

void SystemConfigWindow::setupBottomSection() {
    // 底部按钮区域 - 创建包含所有操作按钮的底部区域
    QFrame* bottomFrame = new QFrame(); // 创建框架容器，用于承载按钮组
    // 设置底部框架样式，透明背景，无边框
    bottomFrame->setStyleSheet(
        "QFrame {"
        "  background: transparent;" // 透明背景，不遮挡内容
        "  border: none;" // 无边框
        "  padding: 0px;" // 无内边距
        "  border-radius: 20px;" // 圆角
        "}"
    );
    QVBoxLayout* bottomLayout = new QVBoxLayout(bottomFrame); // 垂直布局，用于排列按钮
    bottomLayout->setSpacing(0); // 按钮之间的间距
    
    // 创建四个主要操作按钮
    saveBtn_ = new QPushButton("保存"); // 保存配置按钮
    testBtn_ = new QPushButton("测试"); // 测试连接按钮
    guideBtn_ = new QPushButton("指南"); // 配置指南按钮
    backBtn_ = new QPushButton("返回"); // 返回主菜单按钮
    
    // 设置所有按钮的最小高度，让按钮更易点击
    saveBtn_->setMinimumHeight(42); // 保存按钮高度42像素
    testBtn_->setMinimumHeight(42); // 测试按钮高度42像素
    guideBtn_->setMinimumHeight(42); // 指南按钮高度42像素
    backBtn_->setMinimumHeight(42); // 返回按钮高度42像素
    
    // 设置保存按钮样式 - 主要操作按钮，使用渐变背景
    saveBtn_->setStyleSheet(
        "QPushButton {" // 按钮正常状态
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #FF8C00, stop:1 #FF6B35);" // 橙色到红色渐变
        "  color: white;" // 白色文字
        "  border: none;" // 无边框
        "  border-radius: 14px;" // 圆角
        "  padding: 10px 18px;" // 内边距
        "  font-size: 16px;" // 字体大小18像素
        "  font-weight: 700;" // 字体粗细，700是粗体
        "  margin: 0 18px;" // 外边距
        "}"
        "QPushButton:hover {" // 鼠标悬停时样式
        "  background: #FF8C00;" // 纯橙色背景
        "}"
    );
    
    // 设置测试按钮样式 - 次要操作按钮，使用浅色背景
    testBtn_->setStyleSheet(
        "QPushButton {" // 按钮正常状态
        "  background: #f5f6fa;" // 浅灰色背景
        "  color: #222;" // 深色文字
        "  border: 1.5px solid #e0e0e0;" // 边框
        "  border-radius: 14px;" // 圆角
        "  padding: 10px 18px;" // 内边距
        "  font-size: 16px;" // 字体大小18像素
        "  font-weight: 700;" // 字体粗细，700是粗体
        "  margin: 0 18px;" // 外边距
        "}"
        "QPushButton:hover {" // 鼠标悬停时样式
        "  background: #e0e0e0;" // 深灰色背景
        "}"
    );
    
    // 设置指南按钮样式 - 与测试按钮相同
    guideBtn_->setStyleSheet(
        "QPushButton {" // 按钮正常状态
        "  background: #f5f6fa;" // 浅灰色背景
        "  color: #222;" // 深色文字
        "  border: 1.5px solid #e0e0e0;" // 边框
        "  border-radius: 14px;" // 圆角
        "  padding: 10px 18px;" // 内边距
        "  font-size: 16px;" // 字体大小18像素
        "  font-weight: 700;" // 字体粗细，700是粗体
        "  margin: 0 18px;" // 外边距
        "}"
        "QPushButton:hover {" // 鼠标悬停时样式
        "  background: #e0e0e0;" // 深灰色背景
        "}"
    );
    
    // 设置返回按钮样式 - 与测试按钮相同
    backBtn_->setStyleSheet(
        "QPushButton {" // 按钮正常状态
        "  background: #f5f6fa;" // 浅灰色背景
        "  color: #222;" // 深色文字
        "  border: 1.5px solid #e0e0e0;" // 边框
        "  border-radius: 14px;" // 圆角
        "  padding: 10px 18px;" // 内边距
        "  font-size: 16px;" // 字体大小18像素
        "  font-weight: 700;" // 字体粗细，700是粗体
        "  margin: 0 18px;" // 外边距
        "}"
        "QPushButton:hover {" // 鼠标悬停时样式
        "  background: #e0e0e0;" // 深灰色背景
        "}"
    );
    
    // 创建按钮组容器，用于水平排列所有按钮
    QWidget* buttonGroupWidget = new QWidget(); // 按钮组容器
    QHBoxLayout* groupLayout = new QHBoxLayout(buttonGroupWidget); // 水平布局，按钮水平排列
    groupLayout->setSpacing(14); // 按钮之间的间距
    groupLayout->setContentsMargins(0, 0, 0, 0); // 无额外边距
    
    // 将所有按钮添加到按钮组布局中
    groupLayout->addWidget(saveBtn_); // 添加保存按钮
    groupLayout->addWidget(testBtn_); // 添加测试按钮
    groupLayout->addWidget(guideBtn_); // 添加指南按钮
    groupLayout->addWidget(backBtn_); // 添加返回按钮
    
    bottomLayout->addWidget(buttonGroupWidget, 0, Qt::AlignHCenter); // 将按钮组添加到底部布局，居中对齐
    mainLayout_->addWidget(bottomFrame); // 将底部框架添加到主布局中
    
    // 应用轻微阴影效果替代CSS box-shadow
    ShadowEffect::applyShadow(bottomFrame, 12, QColor(0, 0, 0, 15), QPoint(0, 2));
    
    // 连接按钮点击信号到对应的处理函数
    connect(saveBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onSaveConfig); // 保存按钮点击事件
    connect(testBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onTestConnection); // 测试按钮点击事件
    connect(guideBtn_, &QPushButton::clicked, this, &SystemConfigWindow::showConfigGuide); // 指南按钮点击事件
    connect(backBtn_, &QPushButton::clicked, this, &SystemConfigWindow::onBackToMainMenu); // 返回按钮点击事件
    
    buttonGroupWidget->setStyleSheet("background: transparent;");
}

void SystemConfigWindow::loadCurrentConfig() {
    qDebug() << "[loadCurrentConfig] called";
    AsrConfig config = configManager_->loadConfig();
    if (appIdEdit_) {
        appIdEdit_->setText(QString::fromStdString(config.appId));
    }
    if (accessTokenEdit_) {
        QString token = QString::fromStdString(config.accessToken);
        accessTokenEdit_->setText(maskMiddle(token));
    }
    if (secretKeyEdit_) {
        QString key = QString::fromStdString(config.secretKey);
        secretKeyEdit_->setText(maskMiddle(key));
    }
    configModified_ = false;
}

void SystemConfigWindow::updateConfigStatus() {
    // 更新窗口标题状态，显示配置是否被修改
    if (configModified_) { // 如果配置已被修改
        setWindowTitle("系统配置 *"); // 在标题后添加星号，表示有未保存的更改
    } else { // 如果配置未修改
        setWindowTitle("系统配置"); // 显示正常标题
    }
}

void SystemConfigWindow::onSaveConfig() {
    std::cout << "[UI] Save config button clicked" << std::endl;
    
    // 获取当前配置
    AsrConfig config;
    config.appId = appIdEdit_->text().toStdString();
    // Access Token 和 Secret Key 始终从 configManager 读取原始值
    AsrConfig rawConfig = configManager_->loadConfig();
    config.accessToken = rawConfig.accessToken;
    config.secretKey = rawConfig.secretKey;
    
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
        saveBtn_->setText("保存");
        
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
            client.setToken(config.accessToken);
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
    // Access Token 和 Secret Key 始终从 configManager 读取原始值
    AsrConfig rawConfig = configManager_->loadConfig();
    config.accessToken = rawConfig.accessToken;
    config.secretKey = rawConfig.secretKey;
    
    if (config.appId.empty() || config.accessToken.empty()) {
        QMessageBox::warning(this, "配置错误", "请填写App ID和Access Token");
        return;
    }
    
    std::cout << "[ASR-CRED] 开始测试连接..." << std::endl;
    std::cout << "[ASR-CRED] 测试配置:";
    std::cout << "  - App ID: " << config.appId << std::endl;
    std::cout << "  - Access Token: " << Asr::maskSensitiveInfo(config.accessToken, 4, 4) << std::endl;
    std::cout << "  - Secret Key: " << Asr::maskSensitiveInfo(config.secretKey, 4, 4) << std::endl;
    
    // 禁用测试按钮，显示加载状态
    testBtn_->setEnabled(false);
    
    // 使用QtConcurrent在后台线程中执行测试
    QFutureWatcher<bool>* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool success = watcher->result();
        
        // 恢复按钮状态
        testBtn_->setEnabled(true);
        
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
            client.setToken(config.accessToken);
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

void SystemConfigWindow::onConfigChanged() {
    // 当配置发生变化时调用此函数
    configModified_ = true; // 设置配置修改标志为true
    updateConfigStatus(); // 更新窗口标题状态，显示有未保存的更改
}

void SystemConfigWindow::onConfigManagerUpdated() {
    // 配置管理器更新时，重新加载配置到UI界面
    // 这个函数会在配置管理器发出configUpdated信号时被调用
    loadCurrentConfig(); // 重新加载当前配置到输入框中
}

bool SystemConfigWindow::validateConfig() {
    // 验证配置是否有效
    // 检查应用ID是否为空
    if (appIdEdit_->text().trimmed().isEmpty()) { // 获取输入框文字，去除首尾空格，检查是否为空
        return false; // 如果为空，返回false表示配置无效
    }
    
    // 检查访问令牌是否为空
    if (accessTokenEdit_->text().trimmed().isEmpty()) { // 获取输入框文字，去除首尾空格，检查是否为空
        return false; // 如果为空，返回false表示配置无效
    }
    
    return true; // 如果所有必填项都不为空，返回true表示配置有效
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
    qDebug() << "[testAsrConnection] Access Token:" << QString::fromStdString(Asr::maskSensitiveInfo(config.accessToken, 4, 4));
    qDebug() << "[testAsrConnection] Secret Key:" << QString::fromStdString(Asr::maskSensitiveInfo(config.secretKey, 4, 4));
    
    // 调用ASR管理器的测试接口
    bool result = Asr::AsrManager().testConnection(config.appId, config.accessToken, config.secretKey);
    
    if (result) {
        qDebug() << "[testAsrConnection] ASR connection test successful";
    } else {
        qDebug() << "[testAsrConnection] ASR connection test failed";
    }
    
    return result;
}

void SystemConfigWindow::showConfigGuide() {
    // 显示配置指南对话框，帮助用户了解如何配置ASR服务
    QString guide = 
        "🔧 ASR配置指南\n\n" // 标题
        "=== 配置选项说明 ===\n\n" // 配置选项说明标题
        "选项1：申请个人API密钥（推荐，功能完整）\n" // 第一个选项
        "   1. 访问火山引擎官网：https://www.volcengine.com\n" // 步骤1
        "   2. 注册账号并获取您的App ID、Access Token和Secret Key\n" // 步骤3
        "   3. 在配置界面输入这些信息\n" // 步骤4
        "   4. 点击保存并测试连接\n\n" // 步骤5
        "   💰 费用说明：\n" // 费用说明标题
        "   - 具体资费请咨询火山引擎客服\n" // 费用说明内容
        "   - 个人用户可根据使用量选择合适的套餐\n\n" // 费用说明内容
        "   ⚠️ 注意事项：\n" // 注意事项标题
        "   - 申请过程可能需要一定技术基础\n" // 注意事项内容
        "选项2：使用软件提供的体验配置（功能受限）\n" // 第二个选项
        "   - 本软件提供基础的ASR Cloud配置\n" // 选项2说明
        "   - 仅用于软件功能体验和测试\n" // 选项2说明
        "   - 功能会受到一定限制\n" // 选项2说明
        "   ⚠️ 重要说明：\n" // 重要说明标题
        "   - 体验配置仅供功能演示使用\n" // 重要说明内容
        "   - 我们无法长期免费提供相关服务\n" // 重要说明内容
        "- 火山引擎官网：https://www.volcengine.com\n" // 帮助链接
        "- 如有技术问题，请联系火山引擎客服\n" // 帮助信息
        "- 软件使用问题，请查看软件文档或联系开发者"; // 帮助信息
    
    // 显示信息对话框，标题为"配置指南"，内容为上面的guide字符串
    QMessageBox::information(this, "配置指南", guide);
}

bool SystemConfigWindow::eventFilter(QObject* obj, QEvent* event) {
    // 事件过滤器，用于为按钮添加悬停和点击效果
    if (obj == saveBtn_ || obj == testBtn_ || obj == guideBtn_) { // 检查事件是否来自这三个按钮
        if (event->type() == QEvent::Enter) { // 鼠标进入按钮时
            perfx::ui::UIEffectsUtils::applyHoverEffect(static_cast<QWidget*>(obj), QPoint(0, -2), QSize(6, 6), 120); // 应用悬停效果，按钮向上移动2像素，增加6像素大小，动画时长120毫秒
        } else if (event->type() == QEvent::Leave) { // 鼠标离开按钮时
            perfx::ui::UIEffectsUtils::applyHoverEffect(static_cast<QWidget*>(obj), QPoint(0, 0), QSize(0, 0), 120); // 恢复原始状态，动画时长120毫秒
        } else if (event->type() == QEvent::MouseButtonPress) { // 鼠标按下时
            perfx::ui::UIEffectsUtils::applyClickEffect(static_cast<QWidget*>(obj), QPoint(0, 2), QSize(-4, -4), 80); // 应用点击效果，按钮向下移动2像素，减少4像素大小，动画时长80毫秒
        } else if (event->type() == QEvent::MouseButtonRelease) { // 鼠标释放时
            perfx::ui::UIEffectsUtils::applyHoverEffect(static_cast<QWidget*>(obj), QPoint(0, -2), QSize(6, 6), 120); // 恢复悬停效果
        }
    }
    return QWidget::eventFilter(obj, event); // 调用父类的事件过滤器
}

} // namespace perfx
} // namespace ui 