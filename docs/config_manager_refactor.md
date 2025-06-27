# ConfigManager 重构说明

## 问题分析

原始的 `ConfigManager::loadConfig()` 函数存在以下问题：

1. **功能混杂**：一个函数承担了太多责任
   - 环境变量检查
   - 配置文件读取
   - 体验模式配置
   - 调试日志输出
   - 配置验证逻辑

2. **职责不清**：违反了单一职责原则

3. **界面耦合**：包含了UI相关的调试输出和状态管理

## 重构方案

### 1. 功能分离

将原来的 `loadConfig()` 函数拆分为三个独立的类：

#### ConfigLoader - 配置加载器
- **职责**：专门负责配置的加载逻辑
- **功能**：
  - `loadFromEnvironment()` - 从环境变量加载ASR凭证
  - `loadFromUserConfig()` - 从用户配置文件加载配置
  - `loadTrialModeConfig()` - 加载体验模式配置
  - `loadConfigWithPriority()` - 按优先级加载配置

#### ConfigValidator - 配置验证器
- **职责**：专门负责配置验证
- **功能**：
  - `validateAsrConfig()` - 验证ASR配置的完整性
  - `validateAudioConfig()` - 验证音频配置参数
  - `getValidationErrors()` - 获取验证错误信息

#### ConfigManager - 配置管理器（重构后）
- **职责**：配置管理的协调者
- **功能**：
  - 简化的 `loadConfig()` - 调用ConfigLoader
  - 配置保存和状态管理
  - UI交互接口

### 2. 重构后的 loadConfig() 函数

```cpp
AsrConfig ConfigManager::loadConfig() {
    // 使用ConfigLoader按优先级加载配置
    auto [config, source] = ConfigLoader::loadConfigWithPriority(configFilePath_);
    
    // 更新当前配置状态
    currentConfig_ = config;
    currentConfigSource_ = source;
    configLoaded_ = true;
    
    // 输出配置加载结果（可选，用于调试）
    qDebug() << "[ASR-CRED] 配置加载完成:";
    qDebug() << "   - 来源:" << QString::fromStdString(config.configSource);
    qDebug() << "   - App ID:" << QString::fromStdString(config.appId);
    qDebug() << "   - 有效:" << (config.isValid ? "是" : "否");
    
    return config;
}
```

### 3. 配置优先级

重构后保持了原有的配置优先级：

1. **环境变量** (ASR_* 或 VOLC_* 前缀)
2. **用户配置文件** (通过界面设置)
3. **体验模式配置** (厂商提供的混淆配置)

## 优势

1. **单一职责**：每个类都有明确的职责
2. **可测试性**：各个组件可以独立测试
3. **可维护性**：代码结构更清晰，易于维护
4. **可扩展性**：新增配置源或验证规则更容易
5. **解耦合**：UI逻辑与配置加载逻辑分离

## 文件结构

```
src/ui/
├── config_manager.cpp      # ConfigManager实现（重构后）
├── config_loader.cpp       # ConfigLoader实现（新增）
└── config_validator.cpp    # ConfigValidator实现（新增）

include/ui/
└── config_manager.h        # 所有类的声明（重构后）
```

## 使用示例

```cpp
// 使用重构后的ConfigManager
ConfigManager* manager = ConfigManager::instance();
AsrConfig config = manager->loadConfig();

// 直接使用ConfigLoader（如果需要）
auto [config, source] = ConfigLoader::loadConfigWithPriority(configPath);

// 使用ConfigValidator
if (!ConfigValidator::validateAsrConfig(config)) {
    QStringList errors = ConfigValidator::getValidationErrors();
    // 处理验证错误
}
```

## 迁移指南

1. 现有的 `ConfigManager::loadConfig()` 调用无需修改
2. 如果需要更细粒度的控制，可以直接使用 `ConfigLoader` 或 `ConfigValidator`
3. 配置验证逻辑现在由 `ConfigValidator` 统一处理
4. 调试输出可以根据需要调整或移除 