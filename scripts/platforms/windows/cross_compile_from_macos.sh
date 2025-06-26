#!/bin/bash

# =============================================================================
# Windows 交叉编译脚本 - PerfxAgent (在 macOS 上运行)
# =============================================================================
# 
# 本脚本用于在 macOS 系统上交叉编译 Windows 版本的 PerfxAgent
# 使用预编译的 Windows 库，避免复杂的交叉编译依赖问题
# 
# 运行环境要求：
#   - macOS 系统
#   - Homebrew 包管理器
#   - MinGW-w64 交叉编译工具链
#   - 已下载的 Windows 预编译库
# 
# 使用方法：
#   ./scripts/platforms/windows/cross_compile_from_macos.sh [--package] [--validate]
# 
# 选项：
#   --package   生成 Windows 安装包
#   --validate  验证生成的包
# 
# 前置条件：
#   1. 运行下载脚本获取预编译库：
#      ./scripts/platforms/windows/download_prebuilt_libs_from_macos.sh
#   2. 在 Windows 系统上安装 Qt6 等依赖库
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

# 检查依赖
check_dependencies() {
    log_info "Checking cross-compilation dependencies..."
    
    # 检查MinGW-w64
    if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
        log_success "MinGW-w64 found"
    else
        log_error "MinGW-w64 not found. Please install with: brew install mingw-w64"
        log_info "Installation command: brew install mingw-w64"
        exit 1
    fi
    
    # 检查CMake
    if command -v cmake >/dev/null 2>&1; then
        log_success "CMake found"
    else
        log_error "CMake not found"
        exit 1
    fi
    
    # 检查ImageMagick (用于图标生成)
    if command -v convert >/dev/null 2>&1; then
        log_success "ImageMagick found"
    else
        log_warning "ImageMagick not found. Icons may not be generated properly."
    fi
}

# 安装交叉编译依赖
install_cross_dependencies() {
    log_info "Installing cross-compilation dependencies..."
    
    # 检查是否已安装
    if brew list mingw-w64 >/dev/null 2>&1; then
        log_success "MinGW-w64 already installed"
        return
    fi
    
    log_info "Installing MinGW-w64..."
    brew install mingw-w64
    
    if [ $? -eq 0 ]; then
        log_success "MinGW-w64 installed successfully"
    else
        log_error "Failed to install MinGW-w64"
        exit 1
    fi
}

# 检查预编译库
check_prebuilt_libraries() {
    log_info "Checking pre-built Windows libraries..."
    
    local missing_libs=()
    
    # 检查Qt6
    if [ ! -d "$WINDOWS_LIBS_DIR/qt6/installed" ]; then
        missing_libs+=("Qt6")
    else
        log_success "Qt6 pre-built library found"
    fi
    
    # 检查vcpkg库
    if [ ! -d "$WINDOWS_LIBS_DIR/vcpkg/installed/x64-windows" ]; then
        missing_libs+=("vcpkg libraries")
    else
        log_success "vcpkg pre-built libraries found"
    fi
    
    # 检查OpenSSL
    if [ ! -d "$WINDOWS_LIBS_DIR/openssl/installed" ]; then
        missing_libs+=("OpenSSL")
    else
        log_success "OpenSSL pre-built library found"
    fi
    
    if [ ${#missing_libs[@]} -gt 0 ]; then
        log_warning "Missing pre-built libraries: ${missing_libs[*]}"
        log_info "Please run the download script first:"
        log_info "  ./scripts/platforms/windows/download_prebuilt_libs_from_macos.sh"
        log_info "Then install the libraries on a Windows system according to instructions."
        
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    else
        log_success "All pre-built libraries are available"
    fi
}

# 生成Windows图标
generate_windows_icons() {
    log_info "Generating Windows icons..."
    
    if [ -f "scripts/icon_generator.sh" ]; then
        bash scripts/icon_generator.sh
        log_success "Windows icons generated"
    else
        log_warning "Icon generator script not found"
    fi
}

# 创建交叉编译构建目录
setup_build_directory() {
    log_info "Setting up cross-compilation build directory..."
    
    local build_dir="build-windows-cross"
    
    if [ -d "$build_dir" ]; then
        log_info "Cleaning existing build directory..."
        rm -rf "$build_dir"
    fi
    
    mkdir -p "$build_dir"
    log_success "Build directory created: $build_dir"
}

# 配置交叉编译
configure_cross_compile() {
    log_info "Configuring cross-compilation with pre-built libraries..."
    
    cd build-windows-cross
    
    # 检查是否为测试模式
    if [ "$1" = "--test-only" ]; then
        log_info "Running in test mode - skipping Qt6 dependency"
        CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/windows_cross.cmake \
                    -DCMAKE_BUILD_TYPE=Release \
                    -DWINDOWS_PREBUILT_LIBS=TRUE \
                    -DCROSS_COMPILE_WINDOWS=TRUE \
                    -DTEST_MODE=TRUE \
                    -DSKIP_QT6=TRUE"
    else
        # 自动检测Qt6 Windows预编译包路径
        QT6_PREFIX=""
        if [ -f "../third_party/windows_libs/qt6/installed/lib/cmake/Qt6/Qt6Config.cmake" ]; then
            QT6_PREFIX="../third_party/windows_libs/qt6/installed"
            log_success "Detected Qt6 Windows prebuilt: $QT6_PREFIX"
            CMAKE_PREFIX_PATH_ARG="-DCMAKE_PREFIX_PATH=$QT6_PREFIX"
        else
            log_warning "Qt6 Windows prebuilt not found in third_party/windows_libs/qt6/installed. Please check download."
            log_info "You can run with --test-only to skip Qt6 dependency"
            CMAKE_PREFIX_PATH_ARG=""
        fi
        
        CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/windows_cross.cmake \
                    -DCMAKE_BUILD_TYPE=Release \
                    -DWINDOWS_PREBUILT_LIBS=TRUE \
                    -DCROSS_COMPILE_WINDOWS=TRUE \
                    $CMAKE_PREFIX_PATH_ARG"
    fi
    
    # 使用交叉编译配置
    cmake .. $CMAKE_ARGS
    
    if [ $? -eq 0 ]; then
        log_success "Cross-compilation configuration completed"
    else
        log_error "Cross-compilation configuration failed"
        exit 1
    fi
    
    cd ..
}

# 执行交叉编译
build_windows_version() {
    log_info "Building Windows version with pre-built libraries..."
    
    cd build-windows-cross
    
    cmake --build . --config Release --parallel
    
    if [ $? -eq 0 ]; then
        log_success "Windows version built successfully"
    else
        log_error "Windows build failed"
        exit 1
    fi
    
    cd ..
}

# 创建Windows安装包
create_windows_package() {
    log_info "Creating Windows installation package..."
    
    cd build-windows-cross
    
    # 使用CPack创建ZIP包
    cpack -G ZIP
    
    if [ $? -eq 0 ]; then
        log_success "Windows package created successfully"
        
        # 显示生成的包
        log_info "Generated Windows packages:"
        ls -la *.zip 2>/dev/null || echo "No ZIP packages found"
    else
        log_error "Windows package creation failed"
        exit 1
    fi
    
    cd ..
}

# 验证Windows包
validate_windows_package() {
    log_info "Validating Windows package..."
    
    local build_dir="build-windows-cross"
    local zip_file=""
    
    # 查找ZIP文件
    if [ -d "$build_dir" ]; then
        zip_file=$(find "$build_dir" -name "*.zip" -type f | head -1)
    fi
    
    if [ -n "$zip_file" ]; then
        log_success "Found Windows package: $(basename "$zip_file")"
        
        # 检查文件大小
        local size=$(stat -f%z "$zip_file")
        local size_mb=$(echo "scale=1; $size / 1024 / 1024" | bc 2>/dev/null || echo "unknown")
        log_info "Package size: ${size_mb}MB"
        
        # 检查包内容
        log_info "Package contents:"
        unzip -l "$zip_file" | head -20
        
    else
        log_error "Windows package not found"
        exit 1
    fi
}

# 显示使用说明
show_usage_info() {
    echo ""
    echo "🎉 Windows cross-compilation with pre-built libraries completed!"
    echo ""
    echo "📋 Generated files:"
    echo "  • Windows executable: build-windows-cross/bin/PerfxAgent-ASR.exe"
    echo "  • Windows package: build-windows-cross/*.zip"
    echo ""
    echo "✅ Advantages of using pre-built libraries:"
    echo "  • Faster compilation"
    echo "  • More reliable dependencies"
    echo "  • Better compatibility"
    echo "  • Reduced build complexity"
    echo ""
    echo "⚠️  Important notes:"
    echo "  1. This version uses pre-built Windows libraries"
    echo "  2. Test thoroughly on Windows system"
    echo "  3. All dependencies are properly linked"
    echo ""
    echo "🔧 Next steps:"
    echo "  1. Test the package on a Windows system"
    echo "  2. Verify all functionality works correctly"
    echo "  3. The package should be more reliable than pure cross-compilation"
}

# 主函数
main() {
    log_info "Starting Windows cross-compilation with pre-built libraries..."
    
    # 解析命令行参数
    TEST_MODE=false
    PACKAGE_MODE=false
    VALIDATE_MODE=false
    
    for arg in "$@"; do
        case $arg in
            --test-only)
                TEST_MODE=true
                ;;
            --package)
                PACKAGE_MODE=true
                ;;
            --validate)
                VALIDATE_MODE=true
                ;;
        esac
    done
    
    # 检查依赖
    check_dependencies
    
    # 安装交叉编译依赖
    install_cross_dependencies
    
    # 检查预编译库
    check_prebuilt_libraries
    
    # 生成Windows图标
    generate_windows_icons
    
    # 设置构建目录
    setup_build_directory
    
    # 配置交叉编译
    if [ "$TEST_MODE" = true ]; then
        configure_cross_compile --test-only
    else
        configure_cross_compile
    fi
    
    # 执行交叉编译
    build_windows_version
    
    # 创建Windows安装包
    if [ "$PACKAGE_MODE" = true ]; then
        create_windows_package
    fi
    
    # 验证Windows包
    if [ "$VALIDATE_MODE" = true ]; then
        validate_windows_package
    fi
    
    # 显示使用说明
    show_usage_info
    
    log_success "Windows cross-compilation with pre-built libraries completed successfully!"
}

# 运行主函数
main "$@" 