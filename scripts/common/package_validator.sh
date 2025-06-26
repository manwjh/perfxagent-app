#!/bin/bash

# =============================================================================
# 打包校验脚本 - 自动验证打包质量
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

# 全局变量
BUILD_DIR="build"
APP_BUNDLE="$BUILD_DIR/bin/PerfxAgent-ASR.app"
DMG_FILE=""
TGZ_FILE=""
PLATFORM=""
VERSION=""

# 检查结果统计
TOTAL_CHECKS=0
PASSED_CHECKS=0
FAILED_CHECKS=0
WARNINGS=0

# 增加检查计数
increment_check() {
    TOTAL_CHECKS=$((TOTAL_CHECKS + 1))
}

# 记录成功
record_success() {
    PASSED_CHECKS=$((PASSED_CHECKS + 1))
    log_success "$1"
}

# 记录失败
record_failure() {
    FAILED_CHECKS=$((FAILED_CHECKS + 1))
    log_error "$1"
}

# 记录警告
record_warning() {
    WARNINGS=$((WARNINGS + 1))
    log_warning "$1"
}

# 检测平台和版本
detect_platform_and_version() {
    log_info "Detecting platform and version..."
    
    # 查找DMG文件
    DMG_FILES=($(find "$BUILD_DIR" -name "*.dmg" 2>/dev/null))
    if [ ${#DMG_FILES[@]} -gt 0 ]; then
        DMG_FILE="${DMG_FILES[0]}"
        log_info "Found DMG: $(basename "$DMG_FILE")"
        
        # 从文件名提取版本和平台
        FILENAME=$(basename "$DMG_FILE")
        if [[ $FILENAME =~ perfxagent-app-([0-9]+\.[0-9]+\.[0-9]+)-([A-Za-z]+)\.dmg ]]; then
            VERSION="${BASH_REMATCH[1]}"
            PLATFORM="${BASH_REMATCH[2]}"
            log_success "Detected version: $VERSION, platform: $PLATFORM"
        elif [[ $FILENAME == "temp.dmg" ]]; then
            # 如果是temp.dmg，尝试从TGZ文件名获取版本信息
            log_info "Found temp.dmg, trying to get version from TGZ file..."
        fi
    fi
    
    # 查找TGZ文件
    TGZ_FILES=($(find "$BUILD_DIR" -name "*.tar.gz" 2>/dev/null))
    if [ ${#TGZ_FILES[@]} -gt 0 ]; then
        TGZ_FILE="${TGZ_FILES[0]}"
        log_info "Found TGZ: $(basename "$TGZ_FILE")"
        
        # 如果还没有检测到版本，从TGZ文件名提取
        if [ -z "$VERSION" ] || [ -z "$PLATFORM" ]; then
            TGZ_FILENAME=$(basename "$TGZ_FILE")
            if [[ $TGZ_FILENAME =~ perfxagent-app-([0-9]+\.[0-9]+\.[0-9]+)-([A-Za-z]+)\.tar\.gz ]]; then
                VERSION="${BASH_REMATCH[1]}"
                PLATFORM="${BASH_REMATCH[2]}"
                log_success "Detected version from TGZ: $VERSION, platform: $PLATFORM"
            fi
        fi
    fi
    
    increment_check
    if [ -n "$VERSION" ] && [ -n "$PLATFORM" ]; then
        record_success "Platform and version detection completed"
    else
        record_failure "Failed to detect platform and version"
    fi
}

# 检查安装包文件
check_package_files() {
    log_info "Checking package files..."
    
    increment_check
    if [ -f "$DMG_FILE" ]; then
        DMG_SIZE=$(stat -f%z "$DMG_FILE")
        # 使用bc进行大小转换，兼容macOS
        if command -v bc &>/dev/null; then
            DMG_SIZE_KB=$(echo "scale=1; $DMG_SIZE / 1024" | bc)
            DMG_SIZE_MB=$(echo "scale=1; $DMG_SIZE / 1024 / 1024" | bc)
            log_info "DMG size: ${DMG_SIZE_MB}MB (${DMG_SIZE_KB}KB)"
        else
            log_info "DMG size: ${DMG_SIZE} bytes"
        fi
        if [ $DMG_SIZE -gt 50000000 ]; then  # 50MB
            record_success "DMG file exists and size is reasonable"
        else
            record_warning "DMG file is smaller than expected"
        fi
    else
        record_failure "DMG file not found"
    fi
    
    increment_check
    if [ -f "$TGZ_FILE" ]; then
        TGZ_SIZE=$(stat -f%z "$TGZ_FILE")
        # 使用bc进行大小转换，兼容macOS
        if command -v bc &>/dev/null; then
            TGZ_SIZE_KB=$(echo "scale=1; $TGZ_SIZE / 1024" | bc)
            TGZ_SIZE_MB=$(echo "scale=1; $TGZ_SIZE / 1024 / 1024" | bc)
            log_info "TGZ size: ${TGZ_SIZE_MB}MB (${TGZ_SIZE_KB}KB)"
        else
            log_info "TGZ size: ${TGZ_SIZE} bytes"
        fi
        if [ $TGZ_SIZE -gt 50000000 ]; then  # 50MB
            record_success "TGZ file exists and size is reasonable"
        else
            record_warning "TGZ file is smaller than expected"
        fi
    else
        record_failure "TGZ file not found"
    fi
}

# 检查应用程序Bundle
check_app_bundle() {
    log_info "Checking application bundle..."
    
    increment_check
    if [ -d "$APP_BUNDLE" ]; then
        record_success "Application bundle exists"
    else
        record_failure "Application bundle not found: $APP_BUNDLE"
        return 1
    fi
    
    # 检查Bundle结构
    increment_check
    if [ -f "$APP_BUNDLE/Contents/Info.plist" ]; then
        record_success "Info.plist exists"
    else
        record_failure "Info.plist not found"
    fi
    
    increment_check
    if [ -f "$APP_BUNDLE/Contents/MacOS/PerfxAgent-ASR" ]; then
        EXEC_SIZE=$(stat -f%z "$APP_BUNDLE/Contents/MacOS/PerfxAgent-ASR")
        # 使用bc进行大小转换，兼容macOS
        if command -v bc &>/dev/null; then
            EXEC_SIZE_KB=$(echo "scale=1; $EXEC_SIZE / 1024" | bc)
            log_info "Executable size: ${EXEC_SIZE_KB}KB"
        else
            log_info "Executable size: ${EXEC_SIZE} bytes"
        fi
        if [ $EXEC_SIZE -gt 100000 ]; then  # 100KB
            record_success "Executable exists and size is reasonable"
        else
            record_warning "Executable is smaller than expected"
        fi
    else
        record_failure "Executable not found"
    fi
    
    increment_check
    if [ -f "$APP_BUNDLE/Contents/Resources/app_icon.icns" ]; then
        ICON_SIZE=$(stat -f%z "$APP_BUNDLE/Contents/Resources/app_icon.icns")
        # 使用bc进行大小转换，兼容macOS
        if command -v bc &>/dev/null; then
            ICON_SIZE_KB=$(echo "scale=1; $ICON_SIZE / 1024" | bc)
            ICON_SIZE_MB=$(echo "scale=1; $ICON_SIZE / 1024 / 1024" | bc)
            log_info "Icon size: ${ICON_SIZE_MB}MB (${ICON_SIZE_KB}KB)"
        else
            log_info "Icon size: ${ICON_SIZE} bytes"
        fi
        if [ $ICON_SIZE -gt 1000000 ]; then  # 1MB
            record_success "Application icon exists and size is reasonable"
        else
            record_warning "Application icon is smaller than expected"
        fi
    else
        record_failure "Application icon not found"
    fi
}

# 检查可执行文件架构
check_executable_architecture() {
    log_info "Checking executable architecture..."
    
    increment_check
    if [ -f "$APP_BUNDLE/Contents/MacOS/PerfxAgent-ASR" ]; then
        ARCH=$(file "$APP_BUNDLE/Contents/MacOS/PerfxAgent-ASR" | grep -o "arm64\|x86_64")
        if [ "$ARCH" = "arm64" ]; then
            record_success "Executable is ARM64 (Apple Silicon compatible)"
        elif [ "$ARCH" = "x86_64" ]; then
            record_warning "Executable is x86_64 (Intel only)"
        else
            record_failure "Unknown architecture: $ARCH"
        fi
    else
        record_failure "Cannot check architecture - executable not found"
    fi
}

# 检查代码签名
check_code_signature() {
    log_info "Checking code signature..."
    
    increment_check
    if codesign -dv "$APP_BUNDLE" &>/dev/null; then
        SIGNATURE_TYPE=$(codesign -dv "$APP_BUNDLE" 2>&1 | grep "Signature=" | cut -d'=' -f2)
        if [ "$SIGNATURE_TYPE" = "adhoc" ]; then
            record_success "Code signature exists (adhoc - development)"
        else
            record_success "Code signature exists ($SIGNATURE_TYPE)"
        fi
    else
        record_failure "Code signature verification failed"
    fi
}

# 检查依赖库
check_dependencies() {
    log_info "Checking dependencies..."
    
    local frameworks_dir="$APP_BUNDLE/Contents/Frameworks"
    
    # 检查Qt框架
    increment_check
    QT_FRAMEWORKS=$(find "$frameworks_dir" -name "Qt*.framework" 2>/dev/null | wc -l)
    if [ $QT_FRAMEWORKS -ge 10 ]; then
        record_success "Qt frameworks found: $QT_FRAMEWORKS"
    else
        record_warning "Qt frameworks count seems low: $QT_FRAMEWORKS"
    fi
    
    # 检查关键音频库
    increment_check
    local audio_libs=("libportaudio" "libopus" "libogg" "libsndfile")
    local missing_libs=()
    
    for lib in "${audio_libs[@]}"; do
        if ! find "$frameworks_dir" -name "*$lib*" &>/dev/null; then
            missing_libs+=("$lib")
        fi
    done
    
    if [ ${#missing_libs[@]} -eq 0 ]; then
        record_success "All audio libraries found"
    else
        record_failure "Missing audio libraries: ${missing_libs[*]}"
    fi
    
    # 检查OpenSSL
    increment_check
    if find "$frameworks_dir" -name "*libcrypto*" &>/dev/null && find "$frameworks_dir" -name "*libssl*" &>/dev/null; then
        record_success "OpenSSL libraries found"
    else
        record_failure "OpenSSL libraries missing"
    fi
}

# 检查Qt插件
check_qt_plugins() {
    log_info "Checking Qt plugins..."
    
    local plugins_dir="$APP_BUNDLE/Contents/PlugIns"
    
    increment_check
    if [ -d "$plugins_dir/platforms" ]; then
        PLATFORM_PLUGINS=$(find "$plugins_dir/platforms" -name "*.dylib" 2>/dev/null | wc -l)
        if [ $PLATFORM_PLUGINS -gt 0 ]; then
            record_success "Platform plugins found: $PLATFORM_PLUGINS"
        else
            record_failure "No platform plugins found"
        fi
    else
        record_failure "Platform plugins directory not found"
    fi
    
    increment_check
    if [ -d "$plugins_dir/imageformats" ]; then
        IMAGE_PLUGINS=$(find "$plugins_dir/imageformats" -name "*.dylib" 2>/dev/null | wc -l)
        if [ $IMAGE_PLUGINS -gt 0 ]; then
            record_success "Image format plugins found: $IMAGE_PLUGINS"
        else
            record_warning "No image format plugins found"
        fi
    else
        record_warning "Image format plugins directory not found"
    fi
}

# 检查版本信息
check_version_info() {
    log_info "Checking version information..."
    
    increment_check
    if [ -f "$APP_BUNDLE/Contents/Info.plist" ]; then
        BUNDLE_VERSION=$(plutil -extract CFBundleVersion raw "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || echo "")
        if [ "$BUNDLE_VERSION" = "$VERSION" ]; then
            record_success "Bundle version matches: $VERSION"
        else
            record_failure "Bundle version mismatch: expected $VERSION, got $BUNDLE_VERSION"
        fi
    else
        record_failure "Cannot check version - Info.plist not found"
    fi
    
    increment_check
    BUNDLE_ID=$(plutil -extract CFBundleIdentifier raw "$APP_BUNDLE/Contents/Info.plist" 2>/dev/null || echo "")
    if [ "$BUNDLE_ID" = "com.perfx.agent.asr" ]; then
        record_success "Bundle identifier is correct: $BUNDLE_ID"
    else
        record_failure "Bundle identifier is incorrect: $BUNDLE_ID"
    fi
}

# 检查DMG挂载
check_dmg_mount() {
    log_info "Checking DMG mount capability..."
    
    increment_check
    if [ -f "$DMG_FILE" ]; then
        # 尝试挂载DMG
        MOUNT_POINT=$(hdiutil attach "$DMG_FILE" -readonly -nobrowse 2>/dev/null | tail -1 | cut -f3)
        if [ -n "$MOUNT_POINT" ] && [ -d "$MOUNT_POINT" ]; then
            record_success "DMG can be mounted successfully"
            # 检查挂载内容
            if [ -d "$MOUNT_POINT/PerfxAgent-ASR.app" ]; then
                record_success "DMG contains application bundle"
            else
                record_warning "DMG does not contain application bundle"
            fi
            # 卸载DMG
            hdiutil detach "$MOUNT_POINT" &>/dev/null
        else
            record_failure "DMG mount failed"
        fi
    else
        record_failure "Cannot check DMG mount - file not found"
    fi
}

# 生成检查报告
generate_report() {
    log_info "Generating validation report..."
    
    echo ""
    echo "=========================================="
    echo "           PACKAGE VALIDATION REPORT"
    echo "=========================================="
    echo "Platform: $PLATFORM"
    echo "Version: $VERSION"
    echo "Build Directory: $BUILD_DIR"
    echo "Timestamp: $(date)"
    echo ""
    echo "Check Results:"
    echo "  Total Checks: $TOTAL_CHECKS"
    echo "  Passed: $PASSED_CHECKS"
    echo "  Failed: $FAILED_CHECKS"
    echo "  Warnings: $WARNINGS"
    echo ""
    
    if [ $FAILED_CHECKS -eq 0 ]; then
        if [ $WARNINGS -eq 0 ]; then
            log_success "🎉 ALL CHECKS PASSED! Package is ready for distribution."
            exit 0
        else
            log_warning "⚠️  ALL CHECKS PASSED with warnings. Package is ready for distribution."
            exit 0
        fi
    else
        log_error "❌ VALIDATION FAILED! Please fix the issues before distribution."
        exit 1
    fi
}

# 主函数
main() {
    log_info "Starting package validation..."
    
    # 检查构建目录
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        exit 1
    fi
    
    # 执行所有检查
    detect_platform_and_version
    check_package_files
    check_app_bundle
    check_executable_architecture
    check_code_signature
    check_dependencies
    check_qt_plugins
    check_version_info
    check_dmg_mount
    
    # 生成报告
    generate_report
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 