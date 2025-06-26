#!/bin/bash

# PerfxAgent 构建脚本
# 项目根目录的构建入口脚本

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

# 主函数
main() {
    log_info "Starting PerfxAgent build process..."
    
    # 检查脚本是否存在
    local build_script="scripts/build_dev.sh"
    if [ ! -f "$build_script" ]; then
        log_error "Build script not found: $build_script"
        exit 1
    fi
    
    # 检查脚本是否可执行
    if [ ! -x "$build_script" ]; then
        log_warning "Making build script executable..."
        chmod +x "$build_script"
    fi
    
    # 执行构建脚本
    log_info "Executing build script: $build_script"
    ./"$build_script"
    
    log_success "Build process completed!"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 