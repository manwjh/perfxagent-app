#!/bin/bash

# DMG背景设置脚本
# 用于配置DMG安装包的背景和布局

set -e

# 检查参数
if [ $# -lt 2 ]; then
    echo "Usage: $0 <dmg_mount_point> <dmg_background_image>"
    exit 1
fi

DMG_MOUNT_POINT="$1"
DMG_BACKGROUND_IMAGE="$2"

echo "Setting up DMG background and layout..."

# 创建Applications文件夹的符号链接（如果不存在）
if [ ! -L "${DMG_MOUNT_POINT}/Applications" ]; then
    ln -sf /Applications "${DMG_MOUNT_POINT}/Applications"
fi

# 设置背景图片
if [ -f "${DMG_BACKGROUND_IMAGE}" ]; then
    # 创建背景图片目录
    mkdir -p "${DMG_MOUNT_POINT}/.background"
    cp "${DMG_BACKGROUND_IMAGE}" "${DMG_MOUNT_POINT}/.background/background.png"
    
    # 设置背景图片
    osascript << EOF
tell application "Finder"
    tell disk "${DMG_MOUNT_POINT##*/}"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {400, 100, 800, 400}
        set theViewOptions to the icon view options of container window
        set arrangement of theViewOptions to not arranged
        set icon size of theViewOptions to 72
        set background picture of theViewOptions to file ".background:background.png"
        set position of item "PerfxAgent-ASR.app" of container window to {100, 100}
        set position of item "Applications" of container window to {300, 100}
        update without registering applications
        delay 2
        close
    end tell
end tell
EOF
else
    echo "Warning: Background image not found at ${DMG_BACKGROUND_IMAGE}"
fi

echo "DMG setup completed successfully." 