# 平台脚本目录说明

本目录包含针对不同平台的构建、打包和部署脚本。

## 目录结构

```
scripts/platforms/
├── README.md                           # 本说明文档
├── macos/                              # macOS 平台脚本
│   ├── build_package.sh               # macOS 本地构建和打包
│   ├── validate_package.sh            # macOS 包验证
│   └── setup_dmg_ds_store.sh          # DMG 背景设置
└── windows/                            # Windows 平台脚本
    ├── build_package.bat              # Windows 本地构建和打包
    ├── validate_package.bat           # Windows 包验证
    ├── cross_compile_from_macos.sh    # macOS 上交叉编译 Windows 版本
    └── download_prebuilt_libs_from_macos.sh  # macOS 上下载 Windows 预编译库
```

## 脚本说明

### macOS 平台脚本

- **build_package.sh**: 在 macOS 上构建和打包应用程序
- **validate_package.sh**: 验证生成的 macOS 包 (DMG/TGZ)
- **setup_dmg_ds_store.sh**: 设置 DMG 背景和图标

### Windows 平台脚本

- **build_package.bat**: 在 Windows 上构建和打包应用程序
- **validate_package.bat**: 验证生成的 Windows 包 (ZIP)
- **cross_compile_from_macos.sh**: 在 macOS 上交叉编译 Windows 版本
- **download_prebuilt_libs_from_macos.sh**: 在 macOS 上下载 Windows 预编译库

## 使用流程

### macOS 开发流程

1. **本地构建和打包**:
   ```bash
   ./scripts/platforms/macos/build_package.sh
   ```

2. **验证包**:
   ```bash
   ./scripts/platforms/macos/validate_package.sh
   ```

### Windows 开发流程

1. **本地构建和打包** (在 Windows 上):
   ```cmd
   scripts\platforms\windows\build_package.bat
   ```

2. **验证包** (在 Windows 上):
   ```cmd
   scripts\platforms\windows\validate_package.bat
   ```

### 交叉编译流程 (macOS → Windows)

1. **下载预编译库** (在 macOS 上):
   ```bash
   ./scripts/platforms/windows/download_prebuilt_libs_from_macos.sh
   ```

2. **在 Windows 上安装依赖库**:
   - 运行 Qt6 安装器
   - 使用 vcpkg 安装预编译库
   - 按照 `third_party/windows_libs/INSTALL_INSTRUCTIONS.txt` 操作

3. **交叉编译** (在 macOS 上):
   ```bash
   ./scripts/platforms/windows/cross_compile_from_macos.sh --package --validate
   ```

## 注意事项

1. **平台脚本位置**: 脚本按目标平台分类，而不是运行平台
2. **交叉编译**: Windows 目录下的脚本可能需要在 macOS 上运行
3. **依赖管理**: 交叉编译需要预编译库支持
4. **验证**: 每个平台都有对应的包验证脚本

## 扩展说明

- 如需添加新平台，在 `scripts/platforms/` 下创建对应目录
- 遵循现有的命名和结构约定
- 确保脚本有清晰的注释和使用说明 