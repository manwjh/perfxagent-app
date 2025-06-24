#!/bin/bash

# 依赖部署脚本
# 用于部署Qt和其他依赖库到应用程序包中

set -e

# 检查参数
if [ $# -lt 1 ]; then
    echo "Usage: $0 <app_bundle_path>"
    echo "Example: $0 PerfxAgent.app"
    exit 1
fi

APP_BUNDLE="$1"
APP_CONTENTS="${APP_BUNDLE}/Contents"
APP_MACOS="${APP_CONTENTS}/MacOS"
APP_FRAMEWORKS="${APP_CONTENTS}/Frameworks"
APP_PLUGINS="${APP_CONTENTS}/PlugIns"

echo "Deploying dependencies for ${APP_BUNDLE}..."

# 检查应用程序包是否存在
if [ ! -d "${APP_BUNDLE}" ]; then
    echo "Error: Application bundle not found: ${APP_BUNDLE}"
    exit 1
fi

# 创建必要的目录
mkdir -p "${APP_FRAMEWORKS}"
mkdir -p "${APP_PLUGINS}"

# 查找Qt6安装路径
QT6_PREFIX=$(brew --prefix qt@6 2>/dev/null || brew --prefix qt6 2>/dev/null)
if [ -z "${QT6_PREFIX}" ]; then
    echo "Error: Qt6 not found. Please install Qt6 via Homebrew."
    exit 1
fi

echo "Using Qt6 from: ${QT6_PREFIX}"

# 使用macdeployqt部署Qt依赖，但不生成DMG
echo "Deploying Qt dependencies..."
macdeployqt "${APP_BUNDLE}" -verbose=2

# 部署其他依赖库
echo "Deploying additional dependencies..."

# 获取依赖库路径
PORTAUDIO_PREFIX=$(brew --prefix portaudio 2>/dev/null)
OPUS_PREFIX=$(brew --prefix opus 2>/dev/null)
OPENSSL_PREFIX=$(brew --prefix openssl@3 2>/dev/null)
BOOST_PREFIX=$(brew --prefix boost 2>/dev/null)

# 复制PortAudio库
if [ -n "${PORTAUDIO_PREFIX}" ] && [ -f "${PORTAUDIO_PREFIX}/lib/libportaudio.dylib" ]; then
    echo "Deploying PortAudio..."
    cp "${PORTAUDIO_PREFIX}/lib/libportaudio.dylib" "${APP_FRAMEWORKS}/"
    install_name_tool -id "@executable_path/../Frameworks/libportaudio.dylib" "${APP_FRAMEWORKS}/libportaudio.dylib"
    install_name_tool -change "${PORTAUDIO_PREFIX}/lib/libportaudio.dylib" "@executable_path/../Frameworks/libportaudio.dylib" "${APP_MACOS}/PerfxAgent-ASR"
fi

# 复制Opus库
if [ -n "${OPUS_PREFIX}" ] && [ -f "${OPUS_PREFIX}/lib/libopus.dylib" ]; then
    echo "Deploying Opus..."
    cp "${OPUS_PREFIX}/lib/libopus.dylib" "${APP_FRAMEWORKS}/"
    install_name_tool -id "@executable_path/../Frameworks/libopus.dylib" "${APP_FRAMEWORKS}/libopus.dylib"
    install_name_tool -change "${OPUS_PREFIX}/lib/libopus.dylib" "@executable_path/../Frameworks/libopus.dylib" "${APP_MACOS}/PerfxAgent-ASR"
fi

# 复制OpenSSL库
if [ -n "${OPENSSL_PREFIX}" ]; then
    echo "Deploying OpenSSL..."
    for lib in libssl.dylib libcrypto.dylib; do
        if [ -f "${OPENSSL_PREFIX}/lib/${lib}" ]; then
            cp "${OPENSSL_PREFIX}/lib/${lib}" "${APP_FRAMEWORKS}/"
            install_name_tool -id "@executable_path/../Frameworks/${lib}" "${APP_FRAMEWORKS}/${lib}"
            install_name_tool -change "${OPENSSL_PREFIX}/lib/${lib}" "@executable_path/../Frameworks/${lib}" "${APP_MACOS}/PerfxAgent-ASR"
        fi
    done
fi

# 修复所有库的依赖路径
echo "Fixing library dependencies..."
find "${APP_FRAMEWORKS}" -name "*.dylib" -exec install_name_tool -id "@executable_path/../Frameworks/$(basename {})" {} \;

# 修复可执行文件的依赖路径
echo "Fixing executable dependencies..."
for lib in $(otool -L "${APP_MACOS}/PerfxAgent-ASR" | grep -E "(portaudio|opus|ssl|crypto)" | awk '{print $1}' | grep -v "@executable_path"); do
    libname=$(basename "$lib")
    if [ -f "${APP_FRAMEWORKS}/${libname}" ]; then
        install_name_tool -change "$lib" "@executable_path/../Frameworks/${libname}" "${APP_MACOS}/PerfxAgent-ASR"
    fi
done

# 移除代码签名（如果存在）
echo "Removing existing code signatures..."
codesign --remove-signature "${APP_BUNDLE}" 2>/dev/null || true

# 重新签名应用程序包
echo "Re-signing application bundle..."
codesign --force --deep --sign - "${APP_BUNDLE}"

# 验证签名
echo "Verifying code signature..."
codesign --verify --verbose=4 "${APP_BUNDLE}"

# 验证部署
echo "Verifying deployment..."
otool -L "${APP_MACOS}/PerfxAgent-ASR" | head -20

echo "Dependency deployment completed successfully." 