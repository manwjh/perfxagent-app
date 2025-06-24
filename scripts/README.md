# Scripts Directory

This directory contains various utility scripts for the PerfXAgent project.

## Available Scripts

### `build_dev.sh` (推荐 / Recommended)
开发模式构建脚本，自动启用所有调试功能。

**Usage:**
```bash
# 一键开发模式构建
./scripts/build_dev.sh
```

**Features:**
- 自动检查所有依赖
- 清理构建目录
- 以Debug模式构建项目
- 自动设置调试环境变量
- 可选择直接运行应用程序

**Debug Environment Variables:**
- `ASR_DEBUG=1`: 启用ASR模块调试
- `ASR_ENABLE_BUSINESS_LOG=1`: 启用业务逻辑日志
- `ASR_ENABLE_FLOW_LOG=1`: 启用流程控制日志
- `ASR_ENABLE_DATA_LOG=1`: 启用数据处理日志

### `build_package.sh`
生产模式构建和打包脚本。

**Usage:**
```bash
# 生产模式构建
./scripts/build_package.sh

# 调试模式构建
./scripts/build_package.sh --debug

# 查看帮助
./scripts/build_package.sh --help
```

**Features:**
- 支持Release和Debug模式
- 自动创建应用程序图标
- 自动创建DMG背景图片
- 部署所有依赖库
- 生成macOS安装包 (DMG和TGZ格式)
- 验证生成的安装包

### `icon_generator.sh`
从源图像生成各种尺寸和格式的应用程序图标。

**Usage:**
```bash
# 1. 将源图标放置到 resources/icons/PerfxAgent-ASR.png
# 2. 运行脚本
./scripts/icon_generator.sh
```

**What it generates:**
- PNG icons: 16x16, 24x24, 32x32, 48x48, 64x64, 128x128, 256x256, 512x512, 1024x1024
- Windows ICO file: `resources/icons/app_icon.ico`
- macOS ICNS file: `resources/icons/app_icon.icns`

**Requirements:**
- macOS: Built-in `sips` command (no installation needed)
- For better quality: Install ImageMagick (`brew install imagemagick`)
- For ICNS generation: Built-in `iconutil` command

## 构建流程 / Build Workflow

### 开发流程 / Development Workflow
```bash
# 1. 开发模式构建（推荐）
./scripts/build_dev.sh

# 2. 如果需要调试模式构建
./scripts/build_package.sh --debug
```

### 发布流程 / Release Workflow
```bash
# 1. 生产模式构建和打包
./scripts/build_package.sh

# 2. 检查生成的安装包
ls -la build/*.dmg build/*.tar.gz
```

## Adding New Scripts

When adding new scripts:
1. Make them executable: `chmod +x scripts/your_script.sh`
2. Add documentation here
3. Follow the naming convention: `script_name.sh`
4. Include error handling and usage instructions in the script
5. Add color-coded log output for better user experience 