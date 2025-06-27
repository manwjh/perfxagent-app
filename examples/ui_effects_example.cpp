#include "ui/ui_effects_manager.h"
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

using namespace perfx::ui;

/**
 * @brief UI特效使用示例
 * 
 * 这个示例展示了如何使用UIEffectsManager提供的各种UI特效功能：
 * 1. 动画按钮 - 悬停和点击动画效果
 * 2. 图标按钮 - 带阴影和缩放效果的图标按钮
 * 3. 通用动画效果 - 位置、大小、透明度动画
 * 4. 阴影效果 - 可配置的阴影效果
 */
class UIEffectsExample : public QMainWindow {
    Q_OBJECT

public:
    UIEffectsExample(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("UI特效管理器示例");
        setMinimumSize(800, 600);
        
        // 创建中央部件
        QWidget* centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        // 创建主布局
        QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setSpacing(20);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        
        // 添加标题
        QLabel* titleLabel = new QLabel("UI特效管理器示例", this);
        titleLabel->setStyleSheet(
            "QLabel {"
            "  font-size: 24px;"
            "  font-weight: bold;"
            "  color: #333;"
            "  padding: 10px;"
            "}"
        );
        titleLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(titleLabel);
        
        // 1. 动画按钮示例
        createAnimatedButtonExample(mainLayout);
        
        // 2. 图标按钮示例
        createIconButtonExample(mainLayout);
        
        // 3. 通用动画效果示例
        createAnimationEffectExample(mainLayout);
        
        // 4. 阴影效果示例
        createShadowEffectExample(mainLayout);
        
        // 5. 工具函数示例
        createUtilsExample(mainLayout);
    }

private:
    void createAnimatedButtonExample(QVBoxLayout* layout) {
        QGroupBox* groupBox = new QGroupBox("动画按钮示例", this);
        QVBoxLayout* groupLayout = new QVBoxLayout(groupBox);
        
        // 创建默认动画按钮
        UIEffectsManager::AnimatedButton* defaultButton = 
            UIEffectsManager::Utils::createAnimatedButton("默认动画按钮", this);
        groupLayout->addWidget(defaultButton);
        
        // 创建自定义动画按钮
        UIEffectsManager::AnimatedButton* customButton = 
            UIEffectsManager::Utils::createAnimatedButton("自定义动画按钮", this);
        customButton->setHoverOffset(QPoint(0, -5));
        customButton->setHoverScale(QSize(10, 10));
        customButton->setPressOffset(QPoint(0, 2));
        customButton->setPressScale(QSize(-6, -6));
        customButton->setAnimationDuration(200);
        groupLayout->addWidget(customButton);
        
        layout->addWidget(groupBox);
    }
    
    void createIconButtonExample(QVBoxLayout* layout) {
        QGroupBox* groupBox = new QGroupBox("图标按钮示例", this);
        QHBoxLayout* groupLayout = new QHBoxLayout(groupBox);
        
        // 创建默认图标按钮
        QIcon defaultIcon = QIcon(":/icons/app_icon_64x64.png");
        UIEffectsManager::IconButton* defaultIconButton = 
            UIEffectsManager::Utils::createIconButton(defaultIcon, "默认图标", this);
        groupLayout->addWidget(defaultIconButton);
        
        // 创建自定义图标按钮
        UIEffectsManager::IconButton* customIconButton = 
            UIEffectsManager::Utils::createIconButton(defaultIcon, "自定义图标", this);
        customIconButton->setIconSize(QSize(32, 32));
        customIconButton->setButtonSize(QSize(80, 100));
        customIconButton->setShadowEffect(true, 20, 5);
        customIconButton->setHoverScale(1.1);
        groupLayout->addWidget(customIconButton);
        
        layout->addWidget(groupBox);
    }
    
    void createAnimationEffectExample(QVBoxLayout* layout) {
        QGroupBox* groupBox = new QGroupBox("通用动画效果示例", this);
        QHBoxLayout* groupLayout = new QHBoxLayout(groupBox);
        
        // 位置动画
        QPushButton* posButton = new QPushButton("位置动画", this);
        connect(posButton, &QPushButton::clicked, [posButton]() {
            UIEffectsManager::AnimationEffect::animatePosition(
                posButton, posButton->pos() + QPoint(50, 0), 500);
        });
        groupLayout->addWidget(posButton);
        
        // 大小动画
        QPushButton* sizeButton = new QPushButton("大小动画", this);
        connect(sizeButton, &QPushButton::clicked, [sizeButton]() {
            UIEffectsManager::AnimationEffect::animateSize(
                sizeButton, sizeButton->size() + QSize(20, 20), 500);
        });
        groupLayout->addWidget(sizeButton);
        
        // 透明度动画
        QPushButton* opacityButton = new QPushButton("透明度动画", this);
        connect(opacityButton, &QPushButton::clicked, [opacityButton]() {
            UIEffectsManager::AnimationEffect::animateOpacity(opacityButton, 0.5, 500);
        });
        groupLayout->addWidget(opacityButton);
        
        // 缩放动画
        QPushButton* scaleButton = new QPushButton("缩放动画", this);
        connect(scaleButton, &QPushButton::clicked, [scaleButton]() {
            UIEffectsManager::AnimationEffect::animateScale(scaleButton, 1.2, 500);
        });
        groupLayout->addWidget(scaleButton);
        
        layout->addWidget(groupBox);
    }
    
    void createShadowEffectExample(QVBoxLayout* layout) {
        QGroupBox* groupBox = new QGroupBox("阴影效果示例", this);
        QHBoxLayout* groupLayout = new QHBoxLayout(groupBox);
        
        // 默认阴影
        QPushButton* defaultShadowButton = new QPushButton("默认阴影", this);
        UIEffectsManager::ShadowEffect::applyShadow(defaultShadowButton);
        groupLayout->addWidget(defaultShadowButton);
        
        // 自定义阴影
        QPushButton* customShadowButton = new QPushButton("自定义阴影", this);
        UIEffectsManager::ShadowEffect::applyShadow(
            customShadowButton, 25, QColor(255, 0, 0, 80), QPoint(5, 5));
        groupLayout->addWidget(customShadowButton);
        
        // 无阴影按钮
        QPushButton* noShadowButton = new QPushButton("无阴影", this);
        groupLayout->addWidget(noShadowButton);
        
        layout->addWidget(groupBox);
    }
    
    void createUtilsExample(QVBoxLayout* layout) {
        QGroupBox* groupBox = new QGroupBox("工具函数示例", this);
        QHBoxLayout* groupLayout = new QHBoxLayout(groupBox);
        
        // 悬停效果
        QPushButton* hoverButton = new QPushButton("悬停效果", this);
        hoverButton->installEventFilter(this);
        groupLayout->addWidget(hoverButton);
        
        // 点击效果
        QPushButton* clickButton = new QPushButton("点击效果", this);
        connect(clickButton, &QPushButton::clicked, [clickButton]() {
            UIEffectsManager::Utils::applyClickEffect(clickButton);
        });
        groupLayout->addWidget(clickButton);
        
        layout->addWidget(groupBox);
    }
    
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (obj->isWidgetType()) {
            QWidget* widget = qobject_cast<QWidget*>(obj);
            if (event->type() == QEvent::Enter) {
                UIEffectsManager::Utils::applyHoverEffect(widget);
            } else if (event->type() == QEvent::Leave) {
                // 恢复原始状态
                UIEffectsManager::AnimationEffect::animatePosition(widget, widget->pos(), 200);
                UIEffectsManager::AnimationEffect::animateSize(widget, widget->size(), 200);
            }
        }
        return QMainWindow::eventFilter(obj, event);
    }
};

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    UIEffectsExample window;
    window.show();
    
    return app.exec();
}

#include "ui_effects_example.moc" 