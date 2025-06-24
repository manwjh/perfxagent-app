#!/bin/bash

# PerfxAgent macOS 安装包构建脚本
# 完整的构建、测试和打包流程

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

# 检查依赖
check_dependencies() {
    log_info "Checking build dependencies..."
    
    # 检查必要的工具
    local tools=("cmake" "make" "brew" "git")
    for tool in "${tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            log_error "$tool is not installed"
            exit 1
        fi
    done
    
    # 检查Qt6
    if ! brew list qt@6 &> /dev/null && ! brew list qt6 &> /dev/null; then
        log_error "Qt6 is not installed. Please run: brew install qt@6"
        exit 1
    fi
    
    # 检查其他依赖
    local deps=("openssl@3" "portaudio" "boost" "opus" "libogg" "nlohmann-json" "websocketpp" "asio" "opencv" "libsndfile")
    for dep in "${deps[@]}"; do
        if ! brew list "$dep" &> /dev/null; then
            log_warning "$dep is not installed. Installing..."
            brew install "$dep"
        fi
    done
    
    log_success "All dependencies are available"
}

# 清理构建目录
clean_build() {
    log_info "Cleaning build directory..."
    rm -rf build
    mkdir -p build
    log_success "Build directory cleaned"
}

# 构建项目
build_project() {
    log_info "Building project..."
    cd build
    
    # 检查是否指定了构建类型
    local build_type="Release"
    if [ "$1" = "debug" ] || [ "$1" = "--debug" ]; then
        build_type="Debug"
        log_info "Building in Debug mode (development)"
    else
        log_info "Building in Release mode (production)"
    fi
    
    # 配置项目
    log_info "Configuring project with CMake (${build_type})..."
    cmake .. -DCMAKE_BUILD_TYPE=${build_type}
    
    # 编译项目
    log_info "Compiling project..."
    make -j$(sysctl -n hw.ncpu)
    
    cd ..
    log_success "Project built successfully in ${build_type} mode"
}

# 测试应用程序
test_application() {
    log_info "Testing application..."
    
    if [ ! -f "build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR" ]; then
        log_error "Application not found in build/bin/PerfxAgent-ASR.app/Contents/MacOS/"
        exit 1
    fi
    
    # 基本功能测试
    log_info "Running basic functionality test..."
    # 检查文件是否存在和可执行
    if [ -x "build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR" ]; then
        log_success "Application is executable"
    else
        log_warning "Application is not executable"
    fi
    
    log_success "Application test completed"
}

# 创建应用程序图标
create_app_icon() {
    log_info "Creating application icon..."
    
    # 检查是否有新生成的图标文件
    if [ -f "resources/icons/app_icon.icns" ]; then
        log_success "Using existing application icon: resources/icons/app_icon.icns"
        # 复制到cmake目录（如果需要的话）
        cp resources/icons/app_icon.icns cmake/app_icon.icns
    elif [ -f "resources/icons/PerfxAgent-ASR.png" ]; then
        log_info "Generating application icon from source..."
        # 运行图标生成脚本
        ./scripts/icon_generator.sh
        # 复制到cmake目录
        cp resources/icons/app_icon.icns cmake/app_icon.icns
        log_success "Application icon created"
    else
        log_warning "No icon source file found. Using default icon."
        # 创建一个简单的默认图标
        touch cmake/app_icon.icns
    fi
}

# 创建DMG背景图片
create_dmg_background() {
    log_info "Creating DMG background image..."
    
    # 检查是否有背景图片源文件
    if [ -f "resources/dmg_background.png" ]; then
        cp resources/dmg_background.png cmake/dmg_background.png
        log_success "DMG background image created"
    else
        log_warning "No DMG background image found. Using default."
        # 创建一个简单的默认背景
        touch cmake/dmg_background.png
    fi
}

# 部署依赖
deploy_dependencies() {
    log_info "Deploying dependencies..."
    
    if [ ! -d "build/bin/PerfxAgent-ASR.app" ]; then
        log_error "Application bundle not found"
        exit 1
    fi
    
    ./cmake/deploy_dependencies.sh build/bin/PerfxAgent-ASR.app
    log_success "Dependencies deployed"
}

# 创建安装包
create_package() {
    log_info "Creating installation package..."
    cd build
    
    # 使用CPack创建安装包
    cpack -G "DragNDrop;TGZ"
    
    cd ..
    log_success "Installation package created"
}

# 验证安装包
verify_package() {
    log_info "Verifying installation package..."
    
    # 检查生成的包文件 - 修复包名匹配模式
    local packages=($(find build -name "*.dmg" -o -name "*.tar.gz"))
    
    if [ ${#packages[@]} -eq 0 ]; then
        log_error "No installation packages found"
        exit 1
    fi
    
    for package in "${packages[@]}"; do
        log_success "Found package: $(basename "$package")"
        ls -lh "$package"
    done
}

# 主函数
main() {
    log_info "Starting PerfxAgent macOS package build process..."
    
    # 解析命令行参数
    local build_mode="release"
    while [[ $# -gt 0 ]]; do
        case $1 in
            --debug|-d)
                build_mode="debug"
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--debug|--help]"
                echo "  --debug, -d    Build in debug mode (development)"
                echo "  --help, -h     Show this help message"
                echo ""
                echo "Default: Build in release mode (production)"
                exit 0
                ;;
            *)
                log_warning "Unknown option: $1"
                shift
                ;;
        esac
    done
    
    log_info "Build mode: ${build_mode}"
    
    # 检查依赖
    check_dependencies
    
    # 清理构建目录
    clean_build
    
    # 构建项目
    build_project "${build_mode}"
    
    # 测试应用程序
    test_application
    
    # 创建应用程序图标
    create_app_icon
    
    # 创建DMG背景图片
    create_dmg_background
    
    # 部署依赖
    deploy_dependencies
    
    # 创建安装包
    create_package
    
    # 验证安装包
    verify_package
    
    log_success "PerfxAgent macOS package build completed successfully!"
    log_info "Installation packages are available in the build/ directory"
    log_info "Build mode used: ${build_mode}"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 