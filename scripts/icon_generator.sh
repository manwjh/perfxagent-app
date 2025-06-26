#!/bin/bash

# =============================================================================
# 跨平台图标生成脚本 - PerfxAgent
# 支持 macOS ICNS 和 Windows ICO 格式
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
SOURCE_ICON="resources/icons/PerfxAgent-ASR.png"  # 高分辨率源图标
MACOS_OUTPUT_DIR="resources/packaging/macos"
WINDOWS_OUTPUT_DIR="resources/packaging/windows"
COMMON_OUTPUT_DIR="resources/icons"

# 图标尺寸
SIZES=(16 24 32 48 64 128 256 512 1024)

# 检查依赖
check_dependencies() {
    log_info "Checking dependencies..."
    
    # 检查ImageMagick
    if command -v convert &> /dev/null; then
        log_success "ImageMagick found"
        IMAGEMAGICK_AVAILABLE=true
    else
        log_warning "ImageMagick not found. Some features may be limited."
        log_info "Install with: brew install imagemagick (macOS) or apt-get install imagemagick (Linux)"
        IMAGEMAGICK_AVAILABLE=false
    fi
    
    # 检查macOS工具
    if command -v iconutil &> /dev/null; then
        log_success "macOS iconutil found"
        ICONUTIL_AVAILABLE=true
    else
        log_warning "macOS iconutil not found. ICNS generation will be skipped."
        ICONUTIL_AVAILABLE=false
    fi
    
    # 检查sips (macOS内置工具)
    if command -v sips &> /dev/null; then
        log_success "macOS sips found"
        SIPS_AVAILABLE=true
    else
        log_warning "macOS sips not found"
        SIPS_AVAILABLE=false
    fi
}

# 创建输出目录
create_directories() {
    log_info "Creating output directories..."
    
    mkdir -p "$MACOS_OUTPUT_DIR"
    mkdir -p "$WINDOWS_OUTPUT_DIR"
    mkdir -p "$COMMON_OUTPUT_DIR"
    
    log_success "Output directories created"
}

# 检查源图标
check_source_icon() {
    log_info "Checking source icon..."
    
    if [ ! -f "$SOURCE_ICON" ]; then
        log_error "Source icon not found at $SOURCE_ICON"
        log_info "Please place your high-resolution source icon (1024x1024 recommended) at that location"
        exit 1
    fi
    
    # 获取源图标信息
    if [ "$IMAGEMAGICK_AVAILABLE" = true ]; then
        local dimensions=$(identify -format "%wx%h" "$SOURCE_ICON" 2>/dev/null)
        log_success "Source icon found: $dimensions"
    else
        log_success "Source icon found: $SOURCE_ICON"
    fi
}

# 生成PNG图标
generate_png_icons() {
    local output_dir="$1"
    local platform_name="$2"
    
    log_info "Generating $platform_name PNG icons..."
    
    for size in "${SIZES[@]}"; do
        local output_file="$output_dir/app_icon_${size}x${size}.png"
        log_info "Generating $output_file..."
        
        if [ "$IMAGEMAGICK_AVAILABLE" = true ]; then
            convert "$SOURCE_ICON" -resize "${size}x${size}" "$output_file"
        elif [ "$SIPS_AVAILABLE" = true ]; then
            sips -z "$size" "$size" "$SOURCE_ICON" --out "$output_file"
        else
            log_error "No image processing tool available. Please install ImageMagick."
            exit 1
        fi
        
        if [ $? -eq 0 ]; then
            log_success "Generated: $(basename "$output_file")"
        else
            log_error "Failed to generate: $(basename "$output_file")"
            exit 1
        fi
    done
}

# 生成macOS ICNS文件
generate_macos_icns() {
    log_info "Generating macOS ICNS file..."
    
    if [ "$ICONUTIL_AVAILABLE" = false ]; then
        log_warning "Skipping ICNS generation - iconutil not available"
        return
    fi
    
    # 创建iconset目录
    local iconset_dir="$MACOS_OUTPUT_DIR/app_icon.iconset"
    mkdir -p "$iconset_dir"
    
    # 复制图标到iconset，使用macOS命名规范
    cp "$MACOS_OUTPUT_DIR/app_icon_16x16.png" "$iconset_dir/icon_16x16.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_32x32.png" "$iconset_dir/icon_16x16@2x.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_32x32.png" "$iconset_dir/icon_32x32.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_64x64.png" "$iconset_dir/icon_32x32@2x.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_128x128.png" "$iconset_dir/icon_128x128.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_256x256.png" "$iconset_dir/icon_128x128@2x.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_256x256.png" "$iconset_dir/icon_256x256.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_512x512.png" "$iconset_dir/icon_256x256@2x.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_512x512.png" "$iconset_dir/icon_512x512.png"
    cp "$MACOS_OUTPUT_DIR/app_icon_1024x1024.png" "$iconset_dir/icon_512x512@2x.png"
    
    # 生成ICNS文件
    iconutil -c icns "$iconset_dir" -o "$MACOS_OUTPUT_DIR/app_icon.icns"
    
    # 清理iconset目录
    rm -rf "$iconset_dir"
    
    log_success "macOS ICNS file generated: $MACOS_OUTPUT_DIR/app_icon.icns"
}

# 生成Windows ICO文件
generate_windows_ico() {
    log_info "Generating Windows ICO file..."
    
    if [ "$IMAGEMAGICK_AVAILABLE" = false ]; then
        log_warning "Skipping ICO generation - ImageMagick not available"
        return
    fi
    
    # 使用ImageMagick生成ICO文件，包含多个尺寸
    convert "$WINDOWS_OUTPUT_DIR/app_icon_16x16.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon_32x32.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon_48x48.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon_64x64.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon_128x128.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon_256x256.png" \
            "$WINDOWS_OUTPUT_DIR/app_icon.ico"
    
    if [ $? -eq 0 ]; then
        log_success "Windows ICO file generated: $WINDOWS_OUTPUT_DIR/app_icon.ico"
    else
        log_error "Failed to generate Windows ICO file"
        exit 1
    fi
}

# 复制到通用目录
copy_to_common() {
    log_info "Copying icons to common directory for backward compatibility..."
    
    # 复制macOS ICNS
    if [ -f "$MACOS_OUTPUT_DIR/app_icon.icns" ]; then
        cp "$MACOS_OUTPUT_DIR/app_icon.icns" "$COMMON_OUTPUT_DIR/app_icon.icns"
        log_success "Copied ICNS to common directory"
    fi
    
    # 复制Windows ICO
    if [ -f "$WINDOWS_OUTPUT_DIR/app_icon.ico" ]; then
        cp "$WINDOWS_OUTPUT_DIR/app_icon.ico" "$COMMON_OUTPUT_DIR/app_icon.ico"
        log_success "Copied ICO to common directory"
    fi
    
    # 复制PNG图标
    for size in "${SIZES[@]}"; do
        local png_file="$MACOS_OUTPUT_DIR/app_icon_${size}x${size}.png"
        if [ -f "$png_file" ]; then
            cp "$png_file" "$COMMON_OUTPUT_DIR/"
        fi
    done
    
    log_success "All icons copied to common directory"
}

# 显示生成的文件
show_generated_files() {
    log_info "Generated files summary:"
    
    echo ""
    echo "📁 macOS icons ($MACOS_OUTPUT_DIR):"
    if [ -d "$MACOS_OUTPUT_DIR" ]; then
        ls -la "$MACOS_OUTPUT_DIR"/*.png "$MACOS_OUTPUT_DIR"/*.icns 2>/dev/null || echo "  No files found"
    fi
    
    echo ""
    echo "📁 Windows icons ($WINDOWS_OUTPUT_DIR):"
    if [ -d "$WINDOWS_OUTPUT_DIR" ]; then
        ls -la "$WINDOWS_OUTPUT_DIR"/*.png "$WINDOWS_OUTPUT_DIR"/*.ico 2>/dev/null || echo "  No files found"
    fi
    
    echo ""
    echo "📁 Common icons ($COMMON_OUTPUT_DIR):"
    if [ -d "$COMMON_OUTPUT_DIR" ]; then
        ls -la "$COMMON_OUTPUT_DIR"/app_icon.* 2>/dev/null || echo "  No files found"
    fi
}

# 显示使用说明
show_usage_info() {
    echo ""
    echo "🎉 Cross-platform icon generation complete!"
    echo ""
    echo "📋 Usage information:"
    echo "  • macOS app bundle will use: $MACOS_OUTPUT_DIR/app_icon.icns"
    echo "  • Windows app will use: $WINDOWS_OUTPUT_DIR/app_icon.ico"
    echo "  • Common directory (backward compatibility): $COMMON_OUTPUT_DIR/"
    echo ""
    echo "🔄 To regenerate icons:"
    echo "  ./scripts/icon_generator.sh"
    echo ""
    echo "📝 Next steps:"
    echo "  1. Icons are ready for packaging on both platforms"
    echo "  2. Update your source icon at: $SOURCE_ICON"
    echo "  3. Run this script again to regenerate all icons"
}

# 主函数
main() {
    log_info "Starting cross-platform icon generation for PerfxAgent..."
    
    # 检查依赖
    check_dependencies
    
    # 创建输出目录
    create_directories
    
    # 检查源图标
    check_source_icon
    
    # 生成macOS图标
    generate_png_icons "$MACOS_OUTPUT_DIR" "macOS"
    generate_macos_icns
    
    # 生成Windows图标
    generate_png_icons "$WINDOWS_OUTPUT_DIR" "Windows"
    generate_windows_ico
    
    # 复制到通用目录
    copy_to_common
    
    # 显示生成的文件
    show_generated_files
    
    # 显示使用说明
    show_usage_info
    
    log_success "Cross-platform icon generation completed successfully!"
}

# 运行主函数
main "$@" 