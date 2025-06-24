#!/bin/bash

# Developer ID Application 签名和公证测试脚本
# 用于测试完整的代码签名和公证流程

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

# 检查证书
check_certificates() {
    log_info "检查代码签名证书..."
    
    # 检查可用的代码签名身份
    local identities=$(security find-identity -v -p codesigning)
    
    if [ -z "$identities" ] || echo "$identities" | grep -q "0 valid identities found"; then
        log_error "未找到有效的代码签名证书"
        return 1
    else
        log_success "找到代码签名证书:"
        echo "$identities"
        return 0
    fi
}

# 获取证书标识符
get_certificate_identity() {
    log_info "获取证书标识符..."
    
    # 查找Developer ID Application证书
    local identity=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1 | awk '{print $2}' | tr -d '"')
    
    if [ -z "$identity" ]; then
        log_error "未找到Developer ID Application证书"
        return 1
    fi
    
    log_success "找到证书标识符: $identity"
    echo "$identity"
}

# 签名应用程序
sign_application() {
    local app_path="$1"
    local identity="$2"
    
    log_info "签名应用程序: $app_path"
    
    if [ ! -d "$app_path" ]; then
        log_error "应用程序包不存在: $app_path"
        return 1
    fi
    
    # 深度签名应用程序
    codesign --force --deep --sign "$identity" --options runtime "$app_path"
    
    if [ $? -eq 0 ]; then
        log_success "应用程序签名成功"
        
        # 验证签名
        log_info "验证签名..."
        codesign -dv --verbose=4 "$app_path"
        
        # 检查签名状态
        if codesign --verify --verbose=4 "$app_path" 2>&1 | grep -q "valid on disk"; then
            log_success "签名验证通过"
        else
            log_warning "签名验证失败"
            return 1
        fi
    else
        log_error "应用程序签名失败"
        return 1
    fi
}

# 创建签名的DMG
create_signed_dmg() {
    local app_path="$1"
    local dmg_name="$2"
    local identity="$3"
    
    log_info "创建签名的DMG: $dmg_name"
    
    # 创建临时目录
    local temp_dir=$(mktemp -d)
    local app_name=$(basename "$app_path")
    
    # 复制应用程序到临时目录
    cp -R "$app_path" "$temp_dir/"
    
    # 创建DMG
    hdiutil create -volname "$app_name" -srcfolder "$temp_dir" -ov -format UDZO "$dmg_name"
    
    if [ $? -eq 0 ]; then
        log_success "DMG创建成功"
        
        # 签名DMG
        log_info "签名DMG..."
        codesign --force --deep --sign "$identity" --options runtime "$dmg_name"
        
        if [ $? -eq 0 ]; then
            log_success "DMG签名成功"
            
            # 验证DMG签名
            log_info "验证DMG签名..."
            codesign -dv --verbose=4 "$dmg_name"
        else
            log_error "DMG签名失败"
            return 1
        fi
    else
        log_error "DMG创建失败"
        return 1
    fi
    
    # 清理临时目录
    rm -rf "$temp_dir"
}

# 公证应用程序（需要Apple ID和App专用密码）
notarize_application() {
    local file_path="$1"
    local apple_id="$2"
    local app_password="$3"
    
    if [ -z "$apple_id" ] || [ -z "$app_password" ]; then
        log_warning "跳过公证 - 需要Apple ID和App专用密码"
        log_info "要启用公证，请设置环境变量:"
        log_info "export APPLE_ID='your-apple-id@example.com'"
        log_info "export APP_PASSWORD='your-app-specific-password'"
        return 0
    fi
    
    log_info "开始公证应用程序: $file_path"
    
    # 上传进行公证
    local upload_output=$(xcrun altool --notarize-app \
        --primary-bundle-id "com.perfx.agent.asr" \
        --username "$apple_id" \
        --password "$app_password" \
        --file "$file_path" 2>&1)
    
    if echo "$upload_output" | grep -q "RequestUUID"; then
        local request_uuid=$(echo "$upload_output" | grep "RequestUUID" | awk '{print $3}')
        log_success "公证请求已提交，RequestUUID: $request_uuid"
        
        # 等待公证完成
        log_info "等待公证完成..."
        sleep 30
        
        # 检查公证状态
        local status_output=$(xcrun altool --notarization-info "$request_uuid" \
            --username "$apple_id" \
            --password "$app_password" 2>&1)
        
        if echo "$status_output" | grep -q "success"; then
            log_success "公证成功完成"
            
            # 给应用程序添加公证标记
            xcrun stapler staple "$file_path"
            if [ $? -eq 0 ]; then
                log_success "公证标记已添加到应用程序"
            else
                log_warning "添加公证标记失败"
            fi
        else
            log_error "公证失败"
            echo "$status_output"
            return 1
        fi
    else
        log_error "公证上传失败"
        echo "$upload_output"
        return 1
    fi
}

# 测试应用程序
test_application() {
    local app_path="$1"
    
    log_info "测试应用程序: $app_path"
    
    # 检查应用程序是否可执行
    if [ ! -x "$app_path/Contents/MacOS/PerfxAgent-ASR" ]; then
        log_error "应用程序不可执行"
        return 1
    fi
    
    # 检查依赖库
    log_info "检查依赖库..."
    otool -L "$app_path/Contents/MacOS/PerfxAgent-ASR" | head -10
    
    # 检查代码签名
    log_info "检查代码签名..."
    codesign -dv --verbose=4 "$app_path"
    
    log_success "应用程序测试完成"
}

# 主函数
main() {
    log_info "开始Developer ID Application签名和公证测试..."
    
    # 检查参数
    if [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
        echo "Usage: $0 [--notarize]"
        echo "  --notarize    启用公证流程（需要Apple ID和App专用密码）"
        echo ""
        echo "环境变量:"
        echo "  APPLE_ID       Apple ID邮箱地址"
        echo "  APP_PASSWORD   App专用密码"
        echo ""
        echo "示例:"
        echo "  export APPLE_ID='your-apple-id@example.com'"
        echo "  export APP_PASSWORD='your-app-specific-password'"
        echo "  $0 --notarize"
        exit 0
    fi
    
    local enable_notarize=false
    if [ "$1" = "--notarize" ]; then
        enable_notarize=true
    fi
    
    # 检查应用程序是否存在
    local app_path="build/bin/PerfxAgent-ASR.app"
    if [ ! -d "$app_path" ]; then
        log_error "应用程序不存在: $app_path"
        log_info "请先运行 ./scripts/build_package.sh 构建应用程序"
        exit 1
    fi
    
    # 检查证书
    if ! check_certificates; then
        log_error "证书检查失败，无法继续"
        exit 1
    fi
    
    # 获取证书标识符
    local identity=$(get_certificate_identity)
    if [ $? -ne 0 ]; then
        log_error "无法获取证书标识符"
        exit 1
    fi
    
    # 测试原始应用程序
    test_application "$app_path"
    
    # 签名应用程序
    if ! sign_application "$app_path" "$identity"; then
        log_error "应用程序签名失败"
        exit 1
    fi
    
    # 创建签名的DMG
    local dmg_name="build/PerfxAgent-ASR-1.6.2-Signed.dmg"
    if ! create_signed_dmg "$app_path" "$dmg_name" "$identity"; then
        log_error "DMG创建失败"
        exit 1
    fi
    
    # 公证（如果启用）
    if [ "$enable_notarize" = true ]; then
        local apple_id="$APPLE_ID"
        local app_password="$APP_PASSWORD"
        
        # 公证DMG
        notarize_application "$dmg_name" "$apple_id" "$app_password"
        
        # 公证应用程序包
        notarize_application "$app_path" "$apple_id" "$app_password"
    fi
    
    log_success "Developer ID Application签名和公证测试完成！"
    log_info "签名后的文件:"
    log_info "  应用程序: $app_path"
    log_info "  DMG文件: $dmg_name"
    
    if [ "$enable_notarize" = true ]; then
        log_info "注意: 如果公证成功，应用程序将包含公证标记"
    fi
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi 