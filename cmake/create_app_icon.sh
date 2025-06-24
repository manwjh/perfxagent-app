#!/bin/bash

# 应用程序图标生成脚本
# 从PNG图片生成macOS应用程序图标(.icns文件)

set -e

# 检查参数
if [ $# -lt 1 ]; then
    echo "Usage: $0 <input_png_file> [output_icns_file]"
    echo "Example: $0 icon.png app_icon.icns"
    exit 1
fi

INPUT_PNG="$1"
OUTPUT_ICNS="${2:-app_icon.icns}"

# 检查输入文件
if [ ! -f "${INPUT_PNG}" ]; then
    echo "Error: Input PNG file not found: ${INPUT_PNG}"
    exit 1
fi

echo "Creating macOS application icon from ${INPUT_PNG}..."

# 创建临时目录
TEMP_DIR=$(mktemp -d)
trap "rm -rf ${TEMP_DIR}" EXIT

# 创建图标集目录
ICONSET_DIR="${TEMP_DIR}/app.iconset"
mkdir -p "${ICONSET_DIR}"

# 生成不同尺寸的图标
sizes=(16 32 64 128 256 512 1024)

for size in "${sizes[@]}"; do
    # 生成标准尺寸
    sips -z "${size}" "${size}" "${INPUT_PNG}" --out "${ICONSET_DIR}/icon_${size}x${size}.png"
    
    # 生成@2x尺寸（除了1024x1024）
    if [ "${size}" -lt 1024 ]; then
        sips -z "$((size*2))" "$((size*2))" "${INPUT_PNG}" --out "${ICONSET_DIR}/icon_${size}x${size}@2x.png"
    fi
done

# 使用iconutil生成.icns文件
iconutil -c icns "${ICONSET_DIR}" -o "${OUTPUT_ICNS}"

echo "Application icon created successfully: ${OUTPUT_ICNS}"
echo "Icon sizes generated: 16x16, 32x32, 64x64, 128x128, 256x256, 512x512, 1024x1024" 