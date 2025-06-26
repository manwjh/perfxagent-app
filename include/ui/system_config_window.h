#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QComboBox>
#include <QSpinBox>
#include <QProgressBar>
#include <QStatusBar>
#include <QTimer>
#include <QApplication>
#include <QStyle>
#include <QScrollArea>
#include <QFrame>
#include <memory>

namespace perfx {
namespace ui {

class AboutProjectWidget;
class ConfigManager;

/**
 * @brief 系统配置窗口
 * 
 * 提供ASR配置管理和项目信息展示功能
 * 支持环境变量配置和本地配置文件管理
 */
class SystemConfigWindow : public QWidget {
    Q_OBJECT

public:
    explicit SystemConfigWindow(QWidget* parent = nullptr);
    ~SystemConfigWindow();

signals:
    void backToMainMenuRequested();
    void configUpdated();

private slots:
    void onSaveConfig();
    void onTestConnection();
    void onBackToMainMenu();
    void onShowPasswordToggled(bool checked);
    void onConfigChanged();
    void onConfigManagerUpdated();

private:
    void setupUI();
    void setupHeaderSection(QVBoxLayout* contentLayout);
    void setupAsrConfigSection(QVBoxLayout* contentLayout);
    void setupAboutSection(QVBoxLayout* contentLayout);
    void setupBottomSection();
    void setupStatusSection();
    void loadCurrentConfig();
    void updateConfigStatus();
    bool validateConfig();
    bool testAsrConnection();
    void showStatusMessage(const QString& message, bool isError = false);
    void showConfigGuide();

    // UI组件
    QVBoxLayout* mainLayout_;
    QScrollArea* scrollArea_;
    QWidget* contentWidget_;
    QFrame* asrConfigGroup_;
    QFrame* aboutGroup_;
    
    // ASR配置组件 - 只保留三个关键参数
    QLineEdit* appIdEdit_;
    QLineEdit* accessTokenEdit_;
    QLineEdit* secretKeyEdit_;
    QPushButton* showPasswordBtn_;
    
    // 关于项目组件
    AboutProjectWidget* aboutWidget_;
    
    // 按钮和状态
    QPushButton* saveBtn_;
    QPushButton* testBtn_;
    QPushButton* guideBtn_;
    QPushButton* backBtn_;
    QLabel* statusLabel_;
    QProgressBar* testProgressBar_;
    
    // 配置管理器
    ConfigManager* configManager_;
    
    // 配置状态
    bool configModified_;
    QTimer* statusTimer_;

    QHBoxLayout* buttonLayout_;
};

} // namespace perfx
} // namespace ui 