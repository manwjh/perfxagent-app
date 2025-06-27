#!/bin/bash

# PerfxAgent 开发模式构建脚本
# 用于快速构建和测试开发版本

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
    local tools=("cmake" "make" "brew")
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
    
    log_success "All dependencies are available"
}

# 清理构建目录
clean_build() {
    log_info "Cleaning build directory..."
    rm -rf build
    # 确保删除所有可能的构建残留
    rm -rf build/third_party build/src
    mkdir -p build
    log_success "Build directory cleaned"
}

# 构建项目（开发模式）
build_project() {
    log_info "Building project in Debug mode..."
    cd build
    
    # 配置项目 - 强制使用Debug模式，使用--fresh选项避免缓存问题
    log_info "Configuring project with CMake (Debug mode)..."
    cmake .. -DCMAKE_BUILD_TYPE=Debug --fresh
    
    # 编译项目
    log_info "Compiling project..."
    make -j$(sysctl -n hw.ncpu)
    
    cd ..
    log_success "Project built successfully in Debug mode"
}

# 运行应用程序
run_application() {
    log_info "Running application..."
    
    local app_path="build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR"
    if [ ! -f "$app_path" ]; then
        log_error "Application not found: $app_path"
        exit 1
    fi
    
    # 设置开发环境变量
    export ASR_DEBUG=1
    export ASR_ENABLE_BUSINESS_LOG=1
    export ASR_ENABLE_FLOW_LOG=1
    export ASR_ENABLE_DATA_LOG=1
    
    log_info "Starting application with debug mode enabled..."
    log_info "Debug environment variables set:"
    log_info "  - ASR_DEBUG=1"
    log_info "  - ASR_ENABLE_BUSINESS_LOG=1"
    log_info "  - ASR_ENABLE_FLOW_LOG=1"
    log_info "  - ASR_ENABLE_DATA_LOG=1"
    
    # 运行应用程序
    $app_path
}

# 主函数
main() {
    log_info "Starting PerfxAgent development build process..."
    
    # 检查依赖
    check_dependencies
    
    # 清理构建目录
    clean_build
    
    # 构建项目
    build_project
    
    # 询问是否运行应用程序
    echo ""
    read -p "Do you want to run the application now? (y/n): " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        run_application
    else
        log_info "Application built successfully. You can run it manually:"
        log_info "  ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR"
        log_info "  Or with debug mode: export ASR_DEBUG=1 && ./build/bin/PerfxAgent-ASR.app/Contents/MacOS/PerfxAgent-ASR"
    fi
    
    log_success "Development build completed successfully!"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 
