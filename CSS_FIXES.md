# CSS样式修复记录

## 问题描述
在运行过程中出现以下CSS属性警告：
- `Unknown property box-shadow`
- `Unknown property transform`

## 修复内容

### 1. 移除不支持的CSS属性

#### src/ui/system_config_window.cpp
- **移除 `box-shadow`**: 第85行 `box-shadow: 0 2px 4px rgba(0,0,0,0.1);`
- **移除 `transform`**: 第120行 `transform: translateY(-1px);`
- **简化字体族**: 将复杂的字体族简化为 `'Segoe UI', Arial, sans-serif`

#### src/ui/realtime_audio_to_text_window.cpp
- **修复CSS选择器**: 第200行 `.QFrame` → `QFrame`
- **修复渐变语法**: 第243行 `background-color: qlineargradient` → `background: qlineargradient`

#### src/ui/about_project_widget.cpp
- **移除 `text-decoration`**: 第168行 `text-decoration: underline;`

### 2. 修复说明

Qt样式表不支持以下CSS属性：
- `box-shadow`: 阴影效果
- `transform`: CSS变换
- `text-decoration`: 文本装饰

Qt支持的替代方案：
- 阴影效果：可以通过边框和背景色模拟
- 变换效果：可以通过Qt的动画系统实现
- 文本装饰：可以通过字体设置实现下划线效果

### 3. 验证
修复后，运行应用程序应该不再出现CSS属性警告。

## 注意事项
- Qt样式表是CSS的子集，不是所有CSS属性都支持
- 使用Qt样式表时，建议参考Qt官方文档中的支持属性列表
- 对于复杂的视觉效果，可以考虑使用Qt的绘图系统或动画框架 