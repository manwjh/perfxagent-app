#!/bin/bash

# =============================================================================
# 部署依赖库脚本 - PerfxAgent
# 将Windows预编译库部署到交叉编译环境
# =============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

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
CROSS_COMPILE_DIR="/usr/x86_64-w64-mingw32"

# 检查权限
check_permissions() {
    log_info "Checking permissions..."
    
    if [ ! -w "$CROSS_COMPILE_DIR" ]; then
        log_error "No write permission to $CROSS_COMPILE_DIR"
        log_info "Please run with sudo or change permissions"
        exit 1
    fi
    
    log_success "Permissions OK"
}

# 部署Qt6库
deploy_qt6() {
    log_info "Deploying Qt6 libraries..."
    
    local qt_source="$WINDOWS_LIBS_DIR/qt6/installed"
    local qt_dest="$CROSS_COMPILE_DIR/qt6"
    
    if [ ! -d "$qt_source" ]; then
        log_warning "Qt6 not found at $qt_source"
        log_info "Please install Qt6 according to instructions"
        return 1
    fi
    
    # 创建目标目录
    sudo mkdir -p "$qt_dest"
    
    # 复制Qt6库
    sudo cp -r "$qt_source"/* "$qt_dest/"
    
    log_success "Qt6 deployed to $qt_dest"
}

# 部署vcpkg库
deploy_vcpkg_libs() {
    log_info "Deploying vcpkg libraries..."
    
    local vcpkg_source="$WINDOWS_LIBS_DIR/vcpkg/installed/x64-windows"
    local vcpkg_dest="$CROSS_COMPILE_DIR"
    
    if [ ! -d "$vcpkg_source" ]; then
        log_warning "vcpkg libraries not found at $vcpkg_source"
        log_info "Please install vcpkg libraries according to instructions"
        return 1
    fi
    
    # 复制库文件
    sudo cp -r "$vcpkg_source/lib"/* "$vcpkg_dest/lib/" 2>/dev/null || true
    sudo cp -r "$vcpkg_source/include"/* "$vcpkg_dest/include/" 2>/dev/null || true
    sudo cp -r "$vcpkg_source/bin"/* "$vcpkg_dest/bin/" 2>/dev/null || true
    
    log_success "vcpkg libraries deployed"
}

# 部署OpenSSL
deploy_openssl() {
    log_info "Deploying OpenSSL..."
    
    local openssl_source="$WINDOWS_LIBS_DIR/openssl/installed"
    local openssl_dest="$CROSS_COMPILE_DIR"
    
    if [ ! -d "$openssl_source" ]; then
        log_warning "OpenSSL not found at $openssl_source"
        log_info "Please install OpenSSL according to instructions"
        return 1
    fi
    
    # 复制OpenSSL文件
    sudo cp -r "$openssl_source/lib"/* "$openssl_dest/lib/" 2>/dev/null || true
    sudo cp -r "$openssl_source/include"/* "$openssl_dest/include/" 2>/dev/null || true
    sudo cp -r "$openssl_source/bin"/* "$openssl_dest/bin/" 2>/dev/null || true
    
    log_success "OpenSSL deployed"
}

# 创建符号链接
create_symlinks() {
    log_info "Creating symbolic links..."
    
    # Qt6符号链接
    if [ -d "$CROSS_COMPILE_DIR/qt6" ]; then
        sudo ln -sf "$CROSS_COMPILE_DIR/qt6" "$CROSS_COMPILE_DIR/qt"
        log_success "Qt6 symlink created"
    fi
    
    # 库文件符号链接
    cd "$CROSS_COMPILE_DIR/lib"
    
    # 创建常用库的符号链接
    for lib in libportaudio libopus libogg libsndfile libssl libcrypto; do
        if [ -f "${lib}.dll.a" ]; then
            sudo ln -sf "${lib}.dll.a" "${lib}.a" 2>/dev/null || true
        fi
    done
    
    cd - > /dev/null
    
    log_success "Symbolic links created"
}

# 验证部署
verify_deployment() {
    log_info "Verifying deployment..."
    
    local missing=()
    
    # 检查Qt6
    if [ ! -d "$CROSS_COMPILE_DIR/qt6" ] && [ ! -d "$CROSS_COMPILE_DIR/qt" ]; then
        missing+=("Qt6")
    fi
    
    # 检查常用库
    local libs=("libportaudio" "libopus" "libssl" "libcrypto")
    for lib in "${libs[@]}"; do
        if [ ! -f "$CROSS_COMPILE_DIR/lib/${lib}.a" ] && [ ! -f "$CROSS_COMPILE_DIR/lib/${lib}.dll.a" ]; then
            missing+=("$lib")
        fi
    done
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_warning "Missing libraries: ${missing[*]}"
        return 1
    else
        log_success "All libraries deployed successfully"
    fi
}

# 显示部署信息
show_deployment_info() {
    echo ""
    echo "🎉 Dependencies deployment completed!"
    echo ""
    echo "📋 Deployed libraries:"
    echo "  • Qt6: $CROSS_COMPILE_DIR/qt6"
    echo "  • vcpkg libraries: $CROSS_COMPILE_DIR/lib"
    echo "  • OpenSSL: $CROSS_COMPILE_DIR/lib"
    echo ""
    echo "✅ Benefits:"
    echo "  • Libraries available for cross-compilation"
    echo "  • Proper Windows compatibility"
    echo "  • Faster build process"
    echo ""
    echo "🔧 Next steps:"
    echo "  1. Run cross-compilation script"
    echo "  2. Test the generated Windows executable"
    echo "  3. Verify all dependencies are linked correctly"
}

# 主函数
main() {
    log_info "Starting dependencies deployment..."
    
    # 检查权限
    check_permissions
    
    # 部署各种库
    deploy_qt6
    deploy_vcpkg_libs
    deploy_openssl
    
    # 创建符号链接
    create_symlinks
    
    # 验证部署
    verify_deployment
    
    # 显示部署信息
    show_deployment_info
    
    log_success "Dependencies deployment completed successfully!"
}

# 运行主函数
main "$@" 