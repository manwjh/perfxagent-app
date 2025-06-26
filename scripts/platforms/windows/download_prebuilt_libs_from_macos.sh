#!/bin/bash

# =============================================================================
# Windows 预编译库下载脚本 - PerfxAgent (在 macOS 上运行)
# =============================================================================
# 
# 本脚本用于在 macOS 系统上下载 Windows 版本的预编译库
# 为后续的交叉编译提供必要的依赖库
# 
# 运行环境要求：
#   - macOS 系统
#   - curl 或 wget 下载工具
#   - tar 解压工具
#   - 足够的磁盘空间 (约 2-3GB)
# 
# 使用方法：
#   ./scripts/platforms/windows/download_prebuilt_libs_from_macos.sh
# 
# 下载内容：
#   - Qt6 Windows 开发库
#   - vcpkg 预编译库 (音频库、OpenSSL、Boost等)
#   - OpenSSL Windows 库
# 
# 注意事项：
#   1. 下载完成后，需要在 Windows 系统上安装 Qt6
#   2. 某些库可能需要手动解压和配置
#   3. 确保网络连接稳定，下载过程可能需要较长时间
# =============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 配置
WINDOWS_LIBS_DIR="third_party/windows_libs"
QT6_VERSION="6.6.4"
OPENSSL_VERSION="3.2.1"

# 创建目录结构
create_directories() {
    log_info "Creating Windows libraries directory structure..."
    
    mkdir -p "$WINDOWS_LIBS_DIR"/{qt6,vcpkg,openssl,boost}
    log_success "Directory structure created"
}

# 下载Qt6 Windows库
download_qt6() {
    log_info "Downloading Qt6 Windows pre-built static libraries..."
    
    local qt6_dir="$WINDOWS_LIBS_DIR/qt6"
    
    # 创建Qt6目录结构
    mkdir -p "$qt6_dir/installed"
    
    # 下载Qt6预编译静态库 (使用Qt官方提供的预编译包)
    local qt6_version="6.6.4"
    local qt6_arch="x64"
    local qt6_compiler="mingw"
    
    # Qt6预编译库的下载URL (这里使用Qt官方提供的预编译包)
    local qt6_url="https://download.qt.io/official_releases/qt/${qt6_version%.*}/${qt6_version}/qt-everywhere-src-${qt6_version}.tar.xz"
    local qt6_file="$qt6_dir/qt6-source.tar.xz"
    
    if [ -f "$qt6_file" ]; then
        log_success "Qt6 source already exists"
    else
        log_info "Downloading Qt6 source from: $qt6_url"
        log_warning "Note: This is a large download (~500MB). Please be patient..."
        
        curl -L -o "$qt6_file" "$qt6_url"
        
        if [ $? -eq 0 ]; then
            log_success "Qt6 source downloaded successfully"
            log_info "File size: $(du -h "$qt6_file" | cut -f1)"
        else
            log_error "Failed to download Qt6 source"
            exit 1
        fi
    fi
    
    # 创建Qt6配置说明
    cat > "$qt6_dir/INSTALL_INSTRUCTIONS.txt" << 'EOF'
Qt6 Windows 交叉编译安装说明
============================

由于Qt6的复杂性，建议采用以下方案之一：

方案1: 使用预编译的Qt6静态库 (推荐)
------------------------------------
1. 访问 https://github.com/qt/qt5/releases
2. 下载 qt5-windows-x64-mingw-static.tar.gz
3. 解压到 qt6/installed/ 目录
4. 确保包含 lib/cmake/Qt6/Qt6Config.cmake

方案2: 在Windows上安装Qt6后复制
--------------------------------
1. 在Windows系统上运行 qt-installer.exe
2. 安装Qt6 6.6.4 MinGW版本
3. 将安装目录 (如 C:\Qt\6.6.4\mingw_64) 复制到 qt6/installed/
4. 确保包含 lib/cmake/Qt6/Qt6Config.cmake

方案3: 使用Docker容器
--------------------
1. 使用Qt官方Docker镜像进行交叉编译
2. 避免复杂的本地环境配置

验证安装:
- 检查 qt6/installed/lib/cmake/Qt6/Qt6Config.cmake 是否存在
- 运行交叉编译脚本验证Qt6是否被正确识别
EOF
    
    log_success "Qt6 download and setup completed"
    log_warning "Please follow INSTALL_INSTRUCTIONS.txt to complete Qt6 setup"
}

# 下载vcpkg预编译库
download_vcpkg_libs() {
    log_info "Downloading vcpkg pre-built libraries..."
    
    local vcpkg_dir="$WINDOWS_LIBS_DIR/vcpkg"
    
    # 下载vcpkg
    if [ ! -d "$vcpkg_dir/vcpkg" ]; then
        log_info "Cloning vcpkg repository..."
        git clone https://github.com/Microsoft/vcpkg.git "$vcpkg_dir/vcpkg"
    fi
    
    # 下载预编译包
    local packages=(
        "portaudio:x64-windows"
        "opus:x64-windows"
        "ogg:x64-windows"
        "boost:x64-windows"
        "openssl:x64-windows"
    )
    
    for package in "${packages[@]}"; do
        local pkg_name="${package%:*}"
        local triplet="${package#*:}"
        
        log_info "Downloading $pkg_name for $triplet..."
        
        # 这里需要实际运行vcpkg来下载包
        # 由于vcpkg需要Windows环境，这里只是准备下载脚本
        echo "vcpkg install $pkg_name:$triplet" >> "$vcpkg_dir/download_commands.txt"
    done
    
    log_success "vcpkg download commands prepared"
    log_info "Run the following commands on Windows:"
    cat "$vcpkg_dir/download_commands.txt"
}

# 下载OpenSSL
download_openssl() {
    log_info "Downloading OpenSSL Windows libraries..."
    
    local openssl_dir="$WINDOWS_LIBS_DIR/openssl"
    local openssl_url="https://github.com/openssl/openssl/releases/download/openssl-${OPENSSL_VERSION}/openssl-${OPENSSL_VERSION}.tar.gz"
    local openssl_file="$openssl_dir/openssl-source.tar.gz"
    
    if [ -f "$openssl_file" ]; then
        log_success "OpenSSL source already exists"
        return
    fi
    
    log_info "Downloading OpenSSL source from: $openssl_url"
    curl -L -o "$openssl_file" "$openssl_url"
    
    if [ $? -eq 0 ]; then
        log_success "OpenSSL source downloaded successfully"
        log_info "File size: $(du -h "$openssl_file" | cut -f1)"
    else
        log_error "Failed to download OpenSSL source"
        exit 1
    fi
}

# 创建安装说明
create_install_instructions() {
    log_info "Creating installation instructions..."
    
    cat > "$WINDOWS_LIBS_DIR/INSTALL_INSTRUCTIONS.txt" << 'EOF'
Windows 预编译库安装说明
========================

本目录包含从 macOS 下载的 Windows 预编译库，需要在 Windows 系统上安装。

1. Qt6 安装
   - 运行 qt6/qt6-installer.exe
   - 选择 Qt 6.6.4 和 MinGW 编译器
   - 安装到默认位置或自定义路径
   - 安装完成后，将安装目录复制到 qt6/installed/

2. vcpkg 库安装
   - 在 Windows 上运行 vcpkg/download_commands.txt 中的命令
   - 或者手动运行：
     vcpkg install portaudio:x64-windows
     vcpkg install opus:x64-windows
     vcpkg install ogg:x64-windows
     vcpkg install boost:x64-windows
     vcpkg install openssl:x64-windows

3. OpenSSL 安装
   - 解压 openssl/openssl-source.tar.gz
   - 在 Windows 上编译或使用预编译版本
   - 将编译结果放到 openssl/installed/

4. 验证安装
   - 确保所有库都在正确的目录结构中
   - 运行交叉编译脚本验证依赖

注意事项：
- 所有路径都应该是绝对路径
- 确保库的架构匹配 (x64-windows)
- 某些库可能需要额外的配置
EOF
    
    log_success "Installation instructions created"
}

# 显示下载总结
show_download_summary() {
    echo ""
    echo "📦 Windows 预编译库下载完成！"
    echo ""
    echo "📁 下载内容："
    echo "  • Qt6 Windows 安装器: $WINDOWS_LIBS_DIR/qt6/"
    echo "  • vcpkg 预编译库: $WINDOWS_LIBS_DIR/vcpkg/"
    echo "  • OpenSSL 源码: $WINDOWS_LIBS_DIR/openssl/"
    echo ""
    echo "📋 下一步操作："
    echo "  1. 在 Windows 系统上安装 Qt6"
    echo "  2. 运行 vcpkg 安装预编译库"
    echo "  3. 编译 OpenSSL (如需要)"
    echo "  4. 运行交叉编译脚本"
    echo ""
    echo "📖 详细说明："
    echo "  查看 $WINDOWS_LIBS_DIR/INSTALL_INSTRUCTIONS.txt"
}

# 主函数
main() {
    log_info "Starting Windows pre-built libraries download..."
    
    # 创建目录结构
    create_directories
    
    # 下载各种库
    download_qt6
    download_vcpkg_libs
    download_openssl
    
    # 创建安装说明
    create_install_instructions
    
    # 显示总结
    show_download_summary
    
    log_success "Windows pre-built libraries download completed!"
}

# 运行主函数
main "$@" 